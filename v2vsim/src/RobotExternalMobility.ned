package v2vsim;

import inet.mobility.base.MobilityBase;

simple RobotExternalMobility extends MobilityBase
{
    parameters:
        @class(v2vsim::RobotExternalMobility);

        // Initial position (will be overridden by external updates)
        double initialX @unit(m) = default(0m);
        double initialY @unit(m) = default(0m);
        double initialZ @unit(m) = default(0m);

        // External Docker communication
        string externalHost = default("127.0.0.1");
        int externalPort = default(9000);
        int listenPort = default(8000);  // Port to receive position updates

        // Movement parameters
        double maxSpeed @unit(mps) = default(10mps);
}