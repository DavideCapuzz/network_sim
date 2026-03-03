package v2vsim;

simple RobotApp like inet.applications.contract.IApp
{
    parameters:
        @class(RobotApp);
        @display("i=block/app");

        // Wireless V2V communication (same port for all robots for broadcast)
        int localPort = default(5000);
        int remotePort = default(5000);

        // External Docker communication (unique per robot)
        int externalListenPort = default(5000);  // Receives from Docker
        int externalSendPort = default(9000);     // Sends to Docker
        string externalHost = default("127.0.0.1");

    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
}