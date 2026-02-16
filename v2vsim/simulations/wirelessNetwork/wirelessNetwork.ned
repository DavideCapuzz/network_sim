package v2vsim.simulations.wirelessNetwork;

import inet.node.inet.WirelessHost;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.common.IntegratedVisualizer;

network WirelessNetwork
{
    parameters:
        int numRobots = default(2);
        @display("bgb=800,600");

    submodules:
        // IPv4 network configurator
        configurator: Ipv4NetworkConfigurator {
            @display("p=50,50");
        }

        // Radio medium
        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,150");
        }

        // Visualization
        visualizer: IntegratedVisualizer {
            @display("p=50,250");
        }

        // Robots
        robot[numRobots]: WirelessHost {
            parameters:
                mobility.typename = "v2vsim.RobotExternalMobility";
                numApps = 1;
                app[0].typename = "v2vsim.RobotApp";

                // Configure WLAN interface
                numWlanInterfaces = 1;
                wlan[0].typename = "Ieee80211ScalarRadio";
                wlan[0].radio.typename = "Ieee80211ScalarRadio";
            @display("p=100,100");
        }
}
