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
 * @file push_world.cpp
 * Example of pushing data to the world model
 *
 * @author Bernhard Firner
 ******************************************************************************/

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

//TODO Remove this (used for usleep) and replace with C++0x sleep mechanism
#include <unistd.h>

#include <owl/client_world_connection.hpp>
#include <owl/grail_types.hpp>
#include <owl/netbuffer.hpp>
#include <owl/solver_world_connection.hpp>
#include <owl/world_model_protocol.hpp>

using std::deque;
using std::map;
using std::pair;
using std::tuple;
using std::u16string;
using std::vector;

using world_model::Attribute;
using world_model::grail_time;
using world_model::URI;

int main(int ac, char** av) {
  if (3 != ac) {
    std::cerr<<"This program needs 2 arguments:\n";
    std::cerr<<"\t"<<av[0]<<" <world model ip> <solver port>\n";
    return 0;
  }

  //World model IP and ports
  std::string wm_ip(av[1]);
  int solver_port = std::stoi(std::string((av[2])));

  //Set up the solver world model connection
  std::u16string origin = u"example_solver";
  //Declare the data types that this solver will provide.
  //TestTypeA is not transient, TestTypeB is transient
  std::vector<std::pair<u16string, bool>> type_pairs{{u"TestTypeA", false}, {u"TestTypeB", true}};
  SolverWorldModel swm(wm_ip, solver_port, type_pairs, origin);
  if (not swm.connected()) {
    std::cerr<<"Could not connect to the world model as a solver - aborting.\n";
    return 0;
  }

  //Make a couple of solutions and send them to the world model.
  std::vector<SolverWorldModel::AttrUpdate> solutions;
  //Send a three byte solutione "0xdeface" to the world model for an object
  //with name "Test Object A"
  SolverWorldModel::AttrUpdate solnA{u"TestTypeA", world_model::getGRAILTime(), u"Test Object A", vector<uint8_t>{0xde, 0xfa, 0xce}};
  solutions.push_back(solnA);
  //Send a float value to the world model for an object with name "Test Object B"
  //Initialize the solution with an empty data field and then use the 
  //pushBackVal function from the netbuffer header. We could push back
  //any number of objects this way (for instance 3 floats for a 3-axis
  //accelerometer).
  SolverWorldModel::AttrUpdate solnB{u"TestTypeB", world_model::getGRAILTime(), u"Test Object B", vector<uint8_t>()};
  pushBackVal<float>(3.14159, solnB.data);
  solutions.push_back(solnB);
  //The second argument to sendData causes these objects to be created if an
  //object with the given names doesn't already exist.
  swm.sendData(solutions, true);

  return 0;
}

