package v2vsim.simulations.wirelessNetwork;

import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.inet.WirelessHost;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.visualizer.common.IntegratedVisualizer;

network WirelessNetwork
{
    parameters:
        int numRobots;
        @display("bgb=600,600");

    submodules:
        visualizer: IntegratedVisualizer {
            @display("p=50,50");
        }

        configurator: Ipv4NetworkConfigurator {
            @display("p=50,150");
        }

        radioMedium: Ieee80211ScalarRadioMedium {
            @display("p=50,250");
        }

        robot[numRobots]: WirelessHost {
            @display("i=device/wifilaptop");
        }
}
