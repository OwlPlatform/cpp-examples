/*
 * Copyright (c) 2013 Bernhard Firner
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * or visit http://www.gnu.org/licenses/gpl-2.0.html
 */

/*******************************************************************************
 * @file query_world.cpp
 * Example of querying the world model
 *
 * @author Bernhard Firner
 ******************************************************************************/

#include <iostream>

#include <owl/netbuffer.hpp>
//#include <owl/solver_aggregator_connection.hpp>
//#include <owl/solver_world_connection.hpp>
//#include <owl/sample_data.hpp>
//#include <owl/world_model_protocol.hpp>
#include <owl/grail_types.hpp>

#include <owl/client_world_connection.hpp>

//using namespace aggregator_solver;

//using std::pair;

using world_model::Attribute;
using world_model::grail_time;
using world_model::URI;

std::u16string toU16(const std::string& str) {
  return std::u16string(str.begin(), str.end());
}

std::string toString(const std::u16string& str) {
  return std::string(str.begin(), str.end());
}

int main(int arg_count, char** arg_vector) {
  if (arg_count != 3) {
    std::cerr<<"This program needs two arguments: the ip address and client port of a world model\n";
    return 0;
  }

  //World model IP and ports
  std::string wm_ip(arg_vector[1]);
  int client_port = std::stoi(std::string((arg_vector[2])));

  //Set up the solver world model connection;
  std::string origin = "binary_state_solver";

  //Read in type information from the config file
  //Types for the GRAIL world model will be read from the file
  std::vector<std::pair<std::u16string, bool>> type_pairs;

  //Remember what names correspond to what solutions and build a
  //query to find all objects of interest.
  //Use the object to solution map to map transmitters to URIs
  std::map<std::u16string, std::u16string> object_to_solution;
  std::map<pair<uint8_t, uint128_t>, URI> tx_to_uri;
  std::mutex tx_to_uri_mutex;

  std::ifstream config(arg_vector[arg_count-1]);
  if (not config.is_open()) {
    std::cerr<<"Error opening configuration file \""<<arg_vector[arg_count-1]<<"\"\n";
    return 1;
  }
  std::string line;
  //Read in each type and its corresponding name
  while (std::getline(config, line)) {
    std::istringstream is(line);
    std::string obj_class, solution;
    if (is >> obj_class >> solution) {
      //Each switch value is a single byte for on or off but the
      //solution name for each object class is different.
      //Safe this as a non-transient solution type
      //Change underlines into spaces.
      std::transform(obj_class.begin(), obj_class.end(), obj_class.begin(),
          [&](char c) { return c == '_' ? ' ' : c;});
      type_pairs.push_back(std::make_pair(toU16(solution), false));
      std::cerr<<"Class \""<<obj_class<<"\" has solution name \""<<solution<<'\n';
      object_to_solution[toU16(obj_class)] = toU16(solution);
    }
    else {
      std::cerr<<"Couldn't make sense of line: \""<<line<<"\"\n";
    }
  }
  config.close();

  if (object_to_solution.empty()) {
    std::cerr<<"There are no types in the config file - aborting.\n";
    return 1;
  }

  std::cerr<<"Trying to connect to world model as a solver.\n";
  SolverWorldModel swm(wm_ip, solver_port, type_pairs, toU16(origin));
  if (not swm.connected()) {
    std::cerr<<"Could not connect to the world model as a solver - aborting.\n";
    return 0;
  }


  //Subscription rules for the GRAIL aggregator
  //The transmitters to request will be discovered from the world model
  //We need to remember what transmitters we've already requested over here.
  std::map<uint8_t, Rule> phy_to_rule;
  std::map<URI, bool> switch_state;

  //Connect to the aggregator and update it with new rules as the world model
  //provided transmitters of interest
  auto packet_callback = [&](SampleData& s) {
    if (s.valid and 1 == s.sense_data.size()) {
      int switch_value = readPrimitive<uint8_t>(s.sense_data, 0);
      if (0 == switch_value or 255 == switch_value) {
        URI uri;
        {
          std::unique_lock<std::mutex> lck(tx_to_uri_mutex);
          uri = tx_to_uri[std::make_pair(s.physical_layer, s.tx_id)];
        }
        bool switch_on = 255 == switch_value;
        if (switch_state.end() == switch_state.find(uri) or switch_state[uri] != switch_on) {
          switch_state[uri] = switch_on;
          //Use the object to solution map to get the solution name.
          for (auto obj_soln = object_to_solution.begin(); obj_soln != object_to_solution.end(); ++obj_soln) {
            if (uri.find(u"." + obj_soln->first + u".") != std::u16string::npos) {
              SolverWorldModel::AttrUpdate soln{obj_soln->second, world_model::getGRAILTime(), uri, std::vector<uint8_t>()};
              pushBackVal<uint8_t>(switch_on ? 1 : 0, soln.data);
              std::vector<SolverWorldModel::AttrUpdate> solns{soln};
              //Send the data to the world model
              bool retry = true;
              while (retry) {
                try {
                  retry = false;
                  swm.sendData(solns, false);
                }
                catch (std::runtime_error& err) {
                  //Retry if this is just 
                  if (err.what() == std::string("Error sending data over socket: Resource temporarily unavailable")) {
                    std::cerr<<"Experiencing socket slow down with world model connection. Retrying...\n";
                    retry = true;
                  }
                  //Otherwise keep throwing
                  else {
                    throw err;
                  }
                }
              }
              if (switch_on) {
                std::cout<<toString(uri)<<" is "<<toString(obj_soln->second)<<'\n';
              } else {
                std::cout<<toString(uri)<<" is not "<<toString(obj_soln->second)<<'\n';
              }
            }
          }
        }
      }
    }
  };
  SolverAggregator aggregator(servers, packet_callback);


  //Now handle connecting as a client.
  //Search for any IDs with names <anything>.obj_class.<anything>
  URI desired_ids = u".*\\.";
  if (object_to_solution.size() == 1) {
    desired_ids = u".*\\." + object_to_solution.begin()->first + u"\\..*";
  }
  else {
    desired_ids += u"(";
    for (auto I = object_to_solution.begin(); I != object_to_solution.end(); ++I) {
      //Insert a | (OR in regex) into the regex if this isn't the first object
      if (I != object_to_solution.begin()) {
        desired_ids += u"|";
      }
      desired_ids += I->first;
    }
    desired_ids += u")\\..*";
  }

  //Search for any matching IDs with switch sensors
  std::vector<URI> attributes{u"sensor.switch.*"};
  //Update at most once a second
  world_model::grail_time interval = 1000;
  //We will connect to the world model as a client inside of the processing loop below
  //Whenever we are disconnected we will attempt to reconnect.

  std::cerr<<"Trying to connect to world model as a client.\n";
  ClientWorldConnection cwc(wm_ip, client_port);

    //It's probably a good idea to make sure you actually connected to the
    //world model. You can use the reconnect member function to attempt
    //to reconnect if you aren't connected or if you become disconnected
    while (not cwc.connected()) {
      std::cerr<<"Problem connecting to world model. Exiting\n";
      return 0;
    }

    //A current snapshot request gets the current values of URIs and their
    //attributes.  The URIs and attributes of interest are specified by POSIX
    //extended REGEX

    //Search for all uris and get all of their attributes
    //Specify all URIs with the '.*' pattern and specific any attribute by
    //giving a vector with only the '.*' pattern as the second argument
    cout<< "Searching for all URIs and attributes\n";
    Response r = cwm.currentSnapshotRequest(u".*", std::vector<std::u16string>{u".*"});
    //The response object is a promise. The request for data does not block,
    //but the get function will. You can test if a result is available with the
    //ready() member function, or you can block until it is ready by calling
    //the get() member function.
    world_model::WorldState state = r.get();
    //Iterate through URI/std::vector<Attribute> pairs and print out their
    //information.
    for (auto obj : state) {
      std::cout<<"Found uri '"<<toString(obj.first)<<" with attributes:\n";
      for (auto attr : obj.second) {
        std::cout<<"\t"<<toString(attr.name)<<" is "<<attr.data.size()<<" bytes long\n";
        std::cout<<"\t"<<toString(attr.name)<<" was created by "<<toString(attr.origin)<<" at time "<<attr.creation_date<<'\n';
      }
    }


    //Get the locations of mugs with updates every second. Location attributes
    //are xoffset, yoffset, and zoffset so request anything that matches the
    //'.offset' regex pattern
    //Update at most once a second
    world_model::grail_time interval = 1000;
    StepResponse sr = cwc.streamRequest(u".*mug.*", std::vector<std::u16string>{u"location\\..offset"}, interval);
    //Keep processing this request
    while (sr.hasNext()) {
      //Get the world model updates
      world_model::WorldState ws = sr.next();
      //We could alternately say for (auto I : ws) as in the snapshot, but I'll
      //be explicit here just to show all of the object names
      for (const std::pair<world_model::URI, std::vector<world_model::Attribute>>& I : ws) {
        if (I.second.empty()) {
          std::cerr<<toString(I.first)<<" is an empty object.\n";
        }
        else {
          std::cout<<"Found uri '"<<toString(I->first)<<" with attributes:\n";
          for (auto attr : I->second) {
            std::cout<<"\t"<<toString(attr.name)<<" is "<<attr.data.size()<<" bytes long\n";
            std::cout<<"\t"<<toString(attr.name)<<" was created by "<<toString(attr.origin)<<" at time "<<attr.creation_date<<'\n';
          }
        }
      }
    }
  }
}

