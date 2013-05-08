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
#include <owl/grail_types.hpp>

#include <owl/client_world_connection.hpp>

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
  uint16_t client_port = std::stoi(std::string((arg_vector[2])));
  std::cout<<"Will try to connect to world model at address "<<wm_ip<<" and port number "<<client_port<<'\n';


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
  std::cout<< "Searching for all URIs and attributes\n";
  Response r = cwc.currentSnapshotRequest(u".*", std::vector<std::u16string>{u".*"});
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
        std::cout<<"Found uri '"<<toString(I.first)<<" with attributes:\n";
        for (auto attr : I.second) {
          std::cout<<"\t"<<toString(attr.name)<<" is "<<attr.data.size()<<" bytes long\n";
          std::cout<<"\t"<<toString(attr.name)<<" was created by "<<toString(attr.origin)<<" at time "<<attr.creation_date<<'\n';
        }
      }
    }
  }
}

