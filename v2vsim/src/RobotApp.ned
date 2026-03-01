package v2vsim;

simple RobotApp like inet.applications.contract.IApp
{
    parameters:
        @class(RobotApp);
        @display("i=block/app");

        // Communication ports
        int localPort = default(5000);
        int remotePort = default(5000);

        // External Docker communication (not used in simple V2V)
        string externalHost = default("127.0.0.1");
        int externalPort = default(9000);

    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
}