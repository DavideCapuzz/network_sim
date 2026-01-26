package multirobot.src;

import inet.applications.base.ApplicationBase;

RobotApp extends ApplicationBase
{
    parameters:
        @class(RobotApp);

        int localPort = default(5000);
        int destPort = default(5000);

        // External communication
        string externalHost = default("172.25.0.1");  // Docker host
        int externalPort = default(10000);

    gates:
        input socketIn;
        output socketOut;
}