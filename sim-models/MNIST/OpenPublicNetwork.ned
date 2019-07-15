//
// Copyright (C) 2012 OpenSim Ltd
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//


package MNIST;

import inet.networklayer.configurator.ipv4.IPv4NetworkConfigurator;
import inet.node.inet.StandardHost;
import inet.node.internetcloud.InternetCloud;
import ned.DatarateChannel;


network OpenPublicNetwork
{
    @display("bgb=719.63574,1358.2529");
    types:
        channel C extends DatarateChannel
        {
            delay = 0ms;
            datarate = 5Mbps;
        }
    submodules:
        h[100]: StandardHost;
        internet: InternetCloud;
        configurator: IPv4NetworkConfigurator {
            parameters:
                config = xmldoc("configIP.xml");
        }        

    connections:

        for i=0..99 {
            h[i].pppg++ <--> C <--> internet.pppg++;
        }        
}

