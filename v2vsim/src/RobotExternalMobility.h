#pragma once

#include <omnetpp.h>
#include "inet/mobility/contract/IMobility.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/common/geometry/common/Quaternion.h"

namespace v2vsim {

    class RobotExternalMobility : public omnetpp::cSimpleModule, public inet::IMobility
    {
    protected:
        // Linear motion
        inet::Coord currentPosition;
        inet::Coord lastPosition;
        inet::Coord currentVelocity;
        inet::Coord currentAcceleration;

        // Angular motion
        inet::Quaternion currentAngularPosition;
        inet::Quaternion currentAngularVelocity;
        inet::Quaternion currentAngularAcceleration;

        // Constraints
        inet::Coord constraintAreaMin;
        inet::Coord constraintAreaMax;

        double maxSpeed;
        int mobilityId;

        omnetpp::simtime_t lastUpdate;

    protected:
        virtual void initialize(int stage) override;
        virtual int numInitStages() const override { return 1; }
        virtual void handleMessage(omnetpp::cMessage *msg) override;

    public:
        // ===== IMobility interface =====
        virtual int getId() const override;
        virtual double getMaxSpeed() const override;

        virtual const inet::Coord& getCurrentPosition() override;
        virtual const inet::Coord& getCurrentVelocity() override;
        virtual const inet::Coord& getCurrentAcceleration() override;

        virtual const inet::Quaternion& getCurrentAngularPosition() override;
        virtual const inet::Quaternion& getCurrentAngularVelocity() override;
        virtual const inet::Quaternion& getCurrentAngularAcceleration() override;

        virtual const inet::Coord& getConstraintAreaMin() const override;
        virtual const inet::Coord& getConstraintAreaMax() const override;
    };

} // namespace v2vsim
