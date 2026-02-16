package v2vsim;

simple RobotApp like inet.applications.contract.IApp
{
    parameters:
        @class(RobotApp);
        @display("i=block/app");
        int localPort = default(5000);
        int destPort = default(5000);
        string externalHost;
        int externalPort;

    gates:
        input socketIn;
        output socketOut;
}