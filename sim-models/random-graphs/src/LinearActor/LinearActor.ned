//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package SchedStream.src.LinearActor;

import inet.applications.contract.IUDPApp;

simple LinearActor like IUDPApp
{
    parameters:
        int		numIter				= default(1000);        
        string	name				= default("a0");							// actor's name
        string	graph				= default("chain0_baseline.tradf.json");	// graph description
		double  defaultVal			= default(0.0);								// value to replace empty tokens with
		string	replacementPolicy	= default("static");  
		
		int		sinePeriod			= default(10);								// in terms of iterations
		double	sineBase			= default(2.0);
		double	sineAmplitude      	= default(5.0);
    
    gates:
        input	udpIn	@labels(UDPControlInfo/up);
        output	udpOut	@labels(UDPControlInfo/down);    
}
