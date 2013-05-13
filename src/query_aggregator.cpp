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

//Headers for an aggregator connection and for interpreting sample data from
//the aggregator
#include <owl/solver_aggregator_connection.hpp>
#include <owl/sample_data.hpp>

#include <string>

//Just get 100 packets for this demonstration.
int total_rxed = 0;

//We'll connect to the aggregator with a callback to handle new packets as they
//arrive
void packet_callback(SampleData& sample) {
  ++total_rxed;
  std::cout<<"Got a sample from ID "<<sample.tx_id<<" on phy "<<
    (unsigned int)sample.physical_layer<<" with "<<sample.sense_data.size()<<
    " bytes of sensed data\n";
  //The packet also has an rss field (received signal strength) and an
  //rx_timestamp field.
}

int main(int arg_count, char** arg_vector) {
  if (arg_count != 3) {
    std::cerr<<"This program needs two arguments: the ip address and client port of a aggregator\n";
    return 0;
  }

  std::string ip(arg_vector[1]);
  uint16_t    port = std::stoi(std::string((arg_vector[2])));
  SolverAggregator::NetTarget agg{ip, port};

  //The solver aggregator connection will accept a vector of aggregators,
  //allowing you to easily connect to multiple aggregator simultaneously.
  SolverAggregator aggregator(std::vector<SolverAggregator::NetTarget>{agg}, packet_callback);
  //After connecting we should add a new connection rule.
  //Get all data from all physical layers (0 represents any physical layer)
  aggregator_solver::Rule everything_rule;
  everything_rule.physical_layer = 0;
  //Request data updates once a second. If packets arrive faster than that
  //we'll just see the most recent packet every second. The time unit is
  //milliseconds.
  everything_rule.update_interval = 1000.0;
  aggregator.addRules(aggregator_solver::Subscription{everything_rule});

  //Get 100 packets and then quit.
  while (total_rxed < 100);

  return 0;
}

