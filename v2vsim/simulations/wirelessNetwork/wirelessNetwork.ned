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

package v2vsim.simulations.wirelessNetwork;

package v2vsim.simulations.wirelessNetwork;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.physicallayer.wireless.unitdisk.UnitDiskRadioMedium;
import inet.node.inet.WirelessHost;
import inet.visualizer.common.IntegratedVisualizer;

//
// TODO auto-generated type
//
network WirelessNetwork
{
    parameters:
        @display("bgb=600,600");
    submodules:
        visualizer: IntegratedVisualizer;
        configurator: Ipv4NetworkConfigurator;
        radioMedium: UnitDiskRadioMedium {
            @display("p=30,98");
        }
        hostA: WirelessHost{
            @display("p=50,195");
        }
        hostB: WirelessHost {
            @display("p=450,195");
        }
}