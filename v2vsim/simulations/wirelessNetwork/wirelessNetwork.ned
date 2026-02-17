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
                // Use custom mobility and app
                mobility.typename = "v2vsim.RobotExternalMobility";
                numApps = 1;
                app[0].typename = "v2vsim.RobotApp";

                // Configure mobility parameters per robot
                mobility.listenPort = 8000 + index;
                mobility.externalPort = 9000 + index;

                // Configure app parameters per robot
                app[0].localPort = 5000 + index;
                app[0].externalPort = 9000 + index;

                // Configure WLAN interface
                numWlanInterfaces = 1;

            @display("p=100+100*index,300");
        }
}
