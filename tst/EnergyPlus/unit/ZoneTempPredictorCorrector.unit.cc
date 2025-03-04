// EnergyPlus, Copyright (c) 1996-2024, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// EnergyPlus::ZoneTempPredictorCorrector Unit Tests

// Google Test Headers
#include <gtest/gtest.h>

#include "Fixtures/EnergyPlusFixture.hh"

// EnergyPlus Headers
#include <AirflowNetwork/Solver.hpp>
#include <EnergyPlus/Data/EnergyPlusData.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataHeatBalFanSys.hh>
#include <EnergyPlus/DataHeatBalSurface.hh>
#include <EnergyPlus/DataHeatBalance.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/DataRoomAirModel.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/DataSurfaces.hh>
#include <EnergyPlus/DataZoneControls.hh>
#include <EnergyPlus/DataZoneEnergyDemands.hh>
#include <EnergyPlus/DataZoneEquipment.hh>
#include <EnergyPlus/HeatBalanceManager.hh>
#include <EnergyPlus/HybridModel.hh>
#include <EnergyPlus/IOFiles.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/SimulationManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>
#include <EnergyPlus/WeatherManager.hh>
#include <EnergyPlus/ZonePlenum.hh>
#include <EnergyPlus/ZoneTempPredictorCorrector.hh>

using namespace EnergyPlus;
using namespace EnergyPlus::DataHeatBalance;
using namespace EnergyPlus::DataHeatBalFanSys;
using namespace DataStringGlobals;
using namespace EnergyPlus::DataZoneControls;
using namespace EnergyPlus::DataZoneEquipment;
using namespace EnergyPlus::DataZoneEnergyDemands;
using namespace EnergyPlus::DataSizing;
using namespace EnergyPlus::HeatBalanceManager;
using namespace EnergyPlus::ZonePlenum;
using namespace EnergyPlus::ZoneTempPredictorCorrector;
using namespace EnergyPlus::DataLoopNode;
using namespace EnergyPlus::DataSurfaces;
using namespace EnergyPlus::DataEnvironment;
using namespace EnergyPlus::Psychrometrics;
using namespace EnergyPlus::ScheduleManager;
using namespace EnergyPlus::RoomAir;
using namespace EnergyPlus::HybridModel;
using namespace SimulationManager;

TEST_F(EnergyPlusFixture, ZoneTempPredictorCorrector_CorrectZoneHumRatTest)
{

    state->dataHVACGlobal->TimeStepSys = 15.0 / 60.0; // System timestep in hours
    state->dataHVACGlobal->TimeStepSysSec = state->dataHVACGlobal->TimeStepSys * Constant::SecInHour;

    state->dataZoneEquip->ZoneEquipConfig.allocate(1);
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneName = "Zone 1";

    state->dataZoneEquip->ZoneEquipConfig(1).NumInletNodes = 2;
    state->dataZoneEquip->ZoneEquipConfig(1).InletNode.allocate(2);
    state->dataZoneEquip->ZoneEquipConfig(1).InletNode(1) = 1;
    state->dataZoneEquip->ZoneEquipConfig(1).InletNode(2) = 2;
    state->dataZoneEquip->ZoneEquipConfig(1).NumExhaustNodes = 1;
    state->dataZoneEquip->ZoneEquipConfig(1).ExhaustNode.allocate(1);
    state->dataZoneEquip->ZoneEquipConfig(1).ExhaustNode(1) = 3;
    state->dataZoneEquip->ZoneEquipConfig(1).NumReturnNodes = 1;
    state->dataZoneEquip->ZoneEquipConfig(1).ReturnNode.allocate(1);
    state->dataZoneEquip->ZoneEquipConfig(1).ReturnNode(1) = 4;
    state->dataZoneEquip->ZoneEquipConfig(1).FixedReturnFlow.allocate(1);

    state->dataLoopNodes->Node.allocate(5);

    state->dataHeatBal->Zone.allocate(1);
    state->dataHeatBal->Zone(1).Name = state->dataZoneEquip->ZoneEquipConfig(1).ZoneName;
    state->dataSize->ZoneEqSizing.allocate(1);
    state->dataSize->CurZoneEqNum = 1;
    state->dataHeatBal->Zone(1).Multiplier = 1.0;
    state->dataHeatBal->Zone(1).Volume = 1000.0;
    state->dataHeatBal->Zone(1).SystemZoneNodeNumber = 5;
    state->dataHeatBal->Zone(1).ZoneVolCapMultpMoist = 1.0;
    state->dataHeatBalFanSys->SumLatentHTRadSys.allocate(1);
    state->dataHeatBalFanSys->SumLatentHTRadSys(1) = 0.0;
    state->dataHeatBalFanSys->SumLatentPool.allocate(1);
    state->dataHeatBalFanSys->SumLatentPool(1) = 0.0;
    state->dataEnvrn->OutBaroPress = 101325.0;
    state->dataZoneTempPredictorCorrector->spaceHeatBalance.allocate(1);
    state->dataZoneTempPredictorCorrector->zoneHeatBalance.allocate(1);
    auto &thisZoneHB = state->dataZoneTempPredictorCorrector->zoneHeatBalance(1);
    thisZoneHB.ZT = 24.0;

    state->dataHeatBal->space.allocate(1);
    state->dataHeatBal->spaceIntGainDevices.allocate(1);
    state->dataHeatBal->Zone(1).spaceIndexes.emplace_back(1);
    state->dataHeatBal->space(1).HTSurfaceFirst = 1;
    state->dataHeatBal->space(1).HTSurfaceLast = 2;
    state->dataSurface->Surface.allocate(2);

    state->dataZonePlenum->NumZoneReturnPlenums = 0;
    state->dataZonePlenum->NumZoneSupplyPlenums = 0;

    state->dataHeatBal->ZoneAirSolutionAlgo = DataHeatBalance::SolutionAlgo::EulerMethod;

    state->dataRoomAir->AirModel.allocate(1);
    state->dataHeatBal->ZoneIntGain.allocate(1);

    // Case 1 - All flows at the same humrat
    thisZoneHB.W1 = 0.008;
    state->dataLoopNodes->Node(1).MassFlowRate = 0.01; // Zone inlet node 1
    state->dataLoopNodes->Node(1).HumRat = 0.008;
    state->dataLoopNodes->Node(2).MassFlowRate = 0.02; // Zone inlet node 2
    state->dataLoopNodes->Node(2).HumRat = 0.008;
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExhBalanced = 0.0;
    state->dataLoopNodes->Node(3).MassFlowRate = 0.00; // Zone exhaust node 1
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExh = state->dataLoopNodes->Node(3).MassFlowRate;
    state->dataLoopNodes->Node(3).HumRat = thisZoneHB.W1;
    state->dataLoopNodes->Node(4).MassFlowRate = 0.03; // Zone return node
    state->dataLoopNodes->Node(4).HumRat = 0.000;
    state->dataLoopNodes->Node(5).HumRat = 0.000;
    thisZoneHB.airHumRat = 0.008;
    thisZoneHB.OAMFL = 0.0;
    thisZoneHB.VAMFL = 0.0;
    thisZoneHB.EAMFL = 0.0;
    thisZoneHB.EAMFLxHumRat = 0.0;
    thisZoneHB.CTMFL = 0.0;
    state->dataEnvrn->OutHumRat = 0.008;
    thisZoneHB.MixingMassFlowXHumRat = 0.0;
    thisZoneHB.MixingMassFlowZone = 0.0;
    thisZoneHB.MDotOA = 0.0;

    thisZoneHB.correctHumRat(*state, 1);
    EXPECT_NEAR(0.008, state->dataLoopNodes->Node(5).HumRat, 0.00001);

    // Case 2 - Unbalanced exhaust flow
    thisZoneHB.W1 = 0.008;
    state->dataLoopNodes->Node(1).MassFlowRate = 0.01; // Zone inlet node 1
    state->dataLoopNodes->Node(1).HumRat = 0.008;
    state->dataLoopNodes->Node(2).MassFlowRate = 0.02; // Zone inlet node 2
    state->dataLoopNodes->Node(2).HumRat = 0.008;
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExhBalanced = 0.0;
    state->dataLoopNodes->Node(3).MassFlowRate = 0.02; // Zone exhaust node 1
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExh = state->dataLoopNodes->Node(3).MassFlowRate;
    state->dataLoopNodes->Node(3).HumRat = thisZoneHB.W1;
    state->dataLoopNodes->Node(4).MassFlowRate = 0.01; // Zone return node
    state->dataLoopNodes->Node(4).HumRat = thisZoneHB.W1;
    state->dataLoopNodes->Node(5).HumRat = 0.000;
    thisZoneHB.airHumRat = 0.008;
    thisZoneHB.OAMFL = 0.0;
    thisZoneHB.VAMFL = 0.0;
    thisZoneHB.EAMFL = 0.0;
    thisZoneHB.EAMFLxHumRat = 0.0;
    thisZoneHB.CTMFL = 0.0;
    state->dataEnvrn->OutHumRat = 0.004;
    thisZoneHB.MixingMassFlowXHumRat = 0.0;
    thisZoneHB.MixingMassFlowZone = 0.0;
    thisZoneHB.MDotOA = 0.0;

    thisZoneHB.correctHumRat(*state, 1);
    EXPECT_NEAR(0.008, state->dataLoopNodes->Node(5).HumRat, 0.00001);

    // Case 3 - Balanced exhaust flow with proper source flow from mixing
    thisZoneHB.W1 = 0.008;
    state->dataLoopNodes->Node(1).MassFlowRate = 0.01; // Zone inlet node 1
    state->dataLoopNodes->Node(1).HumRat = 0.008;
    state->dataLoopNodes->Node(2).MassFlowRate = 0.02; // Zone inlet node 2
    state->dataLoopNodes->Node(2).HumRat = 0.008;
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExhBalanced = 0.02;
    state->dataLoopNodes->Node(3).MassFlowRate = 0.02; // Zone exhaust node 1
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExh = state->dataLoopNodes->Node(3).MassFlowRate;
    state->dataLoopNodes->Node(3).HumRat = thisZoneHB.W1;
    state->dataLoopNodes->Node(4).MassFlowRate = 0.03; // Zone return node
    state->dataLoopNodes->Node(4).HumRat = thisZoneHB.W1;
    state->dataLoopNodes->Node(5).HumRat = 0.000;
    thisZoneHB.airHumRat = 0.008;
    thisZoneHB.OAMFL = 0.0;
    thisZoneHB.VAMFL = 0.0;
    thisZoneHB.EAMFL = 0.0;
    thisZoneHB.EAMFLxHumRat = 0.0;
    thisZoneHB.CTMFL = 0.0;
    state->dataEnvrn->OutHumRat = 0.004;
    thisZoneHB.MixingMassFlowXHumRat = 0.02 * 0.008;
    thisZoneHB.MixingMassFlowZone = 0.02;
    thisZoneHB.MDotOA = 0.0;

    thisZoneHB.correctHumRat(*state, 1);
    EXPECT_NEAR(0.008, state->dataLoopNodes->Node(5).HumRat, 0.00001);

    // Case 4 - Balanced exhaust flow without source flow from mixing
    thisZoneHB.W1 = 0.008;
    state->dataLoopNodes->Node(1).MassFlowRate = 0.01; // Zone inlet node 1
    state->dataLoopNodes->Node(1).HumRat = 0.008;
    state->dataLoopNodes->Node(2).MassFlowRate = 0.02; // Zone inlet node 2
    state->dataLoopNodes->Node(2).HumRat = 0.008;
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExhBalanced = 0.02;
    state->dataLoopNodes->Node(3).MassFlowRate = 0.02; // Zone exhaust node 1
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneExh = state->dataLoopNodes->Node(3).MassFlowRate;
    state->dataLoopNodes->Node(3).HumRat = thisZoneHB.W1;
    state->dataLoopNodes->Node(4).MassFlowRate = 0.01; // Zone return node
    state->dataLoopNodes->Node(4).HumRat = thisZoneHB.W1;
    state->dataLoopNodes->Node(5).HumRat = 0.000;
    thisZoneHB.airHumRat = 0.008;
    thisZoneHB.OAMFL = 0.0;
    thisZoneHB.VAMFL = 0.0;
    thisZoneHB.EAMFL = 0.0;
    thisZoneHB.EAMFLxHumRat = 0.0;
    thisZoneHB.CTMFL = 0.0;
    state->dataEnvrn->OutHumRat = 0.004;
    thisZoneHB.MixingMassFlowXHumRat = 0.0;
    thisZoneHB.MixingMassFlowZone = 0.0;
    thisZoneHB.MDotOA = 0.0;

    thisZoneHB.correctHumRat(*state, 1);
    EXPECT_NEAR(0.008, state->dataLoopNodes->Node(5).HumRat, 0.00001);

    // Add a section to check #6119 by L. Gu on 5/16/17
    thisZoneHB.correctHumRat(*state, 1);
    EXPECT_NEAR(0.008, state->dataLoopNodes->Node(5).HumRat, 0.00001);

    // Issue 6233
    state->dataHeatBal->Zone(1).IsControlled = true;
    thisZoneHB.correctHumRat(*state, 1);
    EXPECT_NEAR(0.008, state->dataLoopNodes->Node(5).HumRat, 0.00001);
}

TEST_F(EnergyPlusFixture, ZoneTempPredictorCorrector_ReportingTest)
{
    // AUTHOR: R. Raustad, FSEC
    // DATE WRITTEN: Aug 2015

    std::string const idf_objects = delimited_string({
        "Zone,",
        "  Core_top,             !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_top Thermostat,     !- Name",
        "  Core_top,                !- Zone or ZoneList Name",
        "  Single Heating Control Type Sched,  !- Control Type Schedule Name",
        "  ThermostatSetpoint:SingleHeating,  !- Control 1 Object Type",
        "  Core_top HeatSPSched;    !- Control 1 Name",
        " ",
        "Schedule:Compact,",
        "  Single Heating Control Type Sched,  !- Name",
        "  Control Type,            !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,1;          !- Field 3",
        " ",
        "ThermostatSetpoint:SingleHeating,",
        "  Core_top HeatSPSched,    !- Name",
        "  SNGL_HTGSETP_SCH;        !- Heating Setpoint Temperature Schedule Name",
        " ",
        "Schedule:Compact,",
        "  SNGL_HTGSETP_SCH,        !- Name",
        "  Temperature,             !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,15.0;       !- Field 3",
        " ",
        "Zone,",
        "  Core_middle,             !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_middle Thermostat,  !- Name",
        "  Core_middle,             !- Zone or ZoneList Name",
        "  Single Cooling Control Type Sched,  !- Control Type Schedule Name",
        "  ThermostatSetpoint:SingleCooling,  !- Control 1 Object Type",
        "  Core_middle CoolSPSched; !- Control 1 Name",
        " ",
        "Schedule:Compact,",
        "  Single Cooling Control Type Sched,  !- Name",
        "  Control Type,            !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,2;          !- Field 3",
        " ",
        "ThermostatSetpoint:SingleCooling,",
        "  Core_middle CoolSPSched, !- Name",
        "  SNGL_CLGSETP_SCH;        !- Cooling Setpoint Temperature Schedule Name",
        " ",
        "Schedule:Compact,",
        "  SNGL_CLGSETP_SCH,        !- Name",
        "  Temperature,             !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,24.0;       !- Field 3",
        " ",
        "Zone,",
        "  Core_basement,             !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_basement Thermostat,  !- Name",
        "  Core_basement,             !- Zone or ZoneList Name",
        "  Single Cooling Heating Control Type Sched,  !- Control Type Schedule Name",
        "  ThermostatSetpoint:SingleHeatingOrCooling,  !- Control 1 Object Type",
        "  Core_basement CoolHeatSPSched; !- Control 1 Name",
        " ",
        "Schedule:Compact,",
        "  Single Cooling Heating Control Type Sched,  !- Name",
        "  Control Type,            !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,3;          !- Field 3",
        " ",
        "ThermostatSetpoint:SingleHeatingOrCooling,",
        "  Core_basement CoolHeatSPSched, !- Name",
        "  CLGHTGSETP_SCH;             !- Heating Setpoint Temperature Schedule Name",
        " ",
        "Zone,",
        "  Core_bottom,             !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_bottom Thermostat,  !- Name",
        "  Core_bottom,             !- Zone or ZoneList Name",
        "  Dual Zone Control Type Sched,  !- Control Type Schedule Name",
        "  ThermostatSetpoint:DualSetpoint,  !- Control 1 Object Type",
        "  Core_bottom DualSPSched; !- Control 1 Name",
        " ",
        "Schedule:Compact,",
        "  Dual Zone Control Type Sched,  !- Name",
        "  Control Type,            !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,4;          !- Field 3",
        " ",
        "ThermostatSetpoint:DualSetpoint,",
        "  Core_bottom DualSPSched, !- Name",
        "  HTGSETP_SCH,             !- Heating Setpoint Temperature Schedule Name",
        "  CLGSETP_SCH;             !- Cooling Setpoint Temperature Schedule Name",
        " ",
        "Schedule:Compact,",
        "  CLGSETP_SCH,             !- Name",
        "  Temperature,             !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,24.0;       !- Field 3",
        " ",
        "Schedule:Compact,",
        "  HTGSETP_SCH,             !- Name",
        "  Temperature,             !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,15.0;       !- Field 3",
        " ",
        "Schedule:Compact,",
        "  CLGHTGSETP_SCH,          !- Name",
        "  Temperature,             !- Schedule Type Limits Name",
        "  Through: 12/31,          !- Field 1",
        "  For: AllDays,            !- Field 2",
        "  Until: 24:00,24.0;       !- Field 3",
    });

    ASSERT_TRUE(process_idf(idf_objects));

    bool ErrorsFound(false); // If errors detected in input
    GetZoneData(*state, ErrorsFound);
    ASSERT_FALSE(ErrorsFound);

    int HeatZoneNum(1);
    int CoolZoneNum(2);
    int CoolHeatZoneNum(3);
    int DualZoneNum(4);

    state->dataGlobal->NumOfTimeStepInHour = 1; // must initialize this to get schedules initialized
    state->dataGlobal->MinutesPerTimeStep = 60; // must initialize this to get schedules initialized
    ProcessScheduleInput(*state);               // read schedules

    GetZoneAirSetPoints(*state);

    state->dataZoneEnergyDemand->DeadBandOrSetback.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataZoneEnergyDemand->CurDeadBandOrSetback.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->TempControlType.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->TempControlTypeRpt.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->TempZoneThermostatSetPoint.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataZoneEnergyDemand->Setback.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->ZoneThermostatSetPointLo.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->ZoneThermostatSetPointHi.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataZoneTempPredictorCorrector->zoneHeatBalance.allocate(state->dataGlobal->NumOfZones);
    state->dataZoneTempPredictorCorrector->spaceHeatBalance.allocate(state->dataGlobal->NumOfZones);

    state->dataHeatBalFanSys->LoadCorrectionFactor.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->LoadCorrectionFactor(HeatZoneNum) = 1.0;
    state->dataHeatBalFanSys->LoadCorrectionFactor(CoolZoneNum) = 1.0;
    state->dataHeatBalFanSys->LoadCorrectionFactor(CoolHeatZoneNum) = 1.0;
    state->dataHeatBalFanSys->LoadCorrectionFactor(DualZoneNum) = 1.0;

    // The following parameters describe the setpoint types in TempControlType(ActualZoneNum)
    //	extern int const SingleHeatingSetPoint; = 1
    //	extern int const SingleCoolingSetPoint; = 2
    //	extern int const SingleHeatCoolSetPoint; = 3
    //	extern int const DualSetPointWithDeadBand; = 4
    state->dataScheduleMgr->Schedule(state->dataZoneCtrls->TempControlledZone(HeatZoneNum).CTSchedIndex).CurrentValue =
        static_cast<int>(HVAC::ThermostatType::SingleHeating);
    state->dataScheduleMgr->Schedule(state->dataZoneCtrls->TempControlledZone(CoolZoneNum).CTSchedIndex).CurrentValue =
        static_cast<int>(HVAC::ThermostatType::SingleCooling);
    state->dataScheduleMgr->Schedule(state->dataZoneCtrls->TempControlledZone(CoolHeatZoneNum).CTSchedIndex).CurrentValue =
        static_cast<int>(HVAC::ThermostatType::SingleHeatCool);

    state->dataScheduleMgr->Schedule(state->dataZoneCtrls->TempControlledZone(DualZoneNum).CTSchedIndex).CurrentValue =
        0; // simulate no thermostat or non-controlled zone

    state->dataZoneEnergyDemand->ZoneSysEnergyDemand(DualZoneNum).TotalOutputRequired =
        0.0; // no load and no thermostat since control type is set to 0 above
    CalcZoneAirTempSetPoints(*state);
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(DualZoneNum).calcPredictedSystemLoad(*state, 1.0, DualZoneNum);

    EXPECT_EQ(0.0,
              state->dataHeatBalFanSys->TempZoneThermostatSetPoint(
                  DualZoneNum)); // Set point initialized to 0 and never set since thermostat control type = 0

    state->dataScheduleMgr->Schedule(state->dataZoneCtrls->TempControlledZone(DualZoneNum).CTSchedIndex).CurrentValue =
        static_cast<int>(HVAC::ThermostatType::DualSetPointWithDeadBand); // reset Tstat control schedule to dual thermostat control

    // set up a back calculated load
    // for the first few, TempIndZnLd() = 0.0
    // LoadToHeatingSetPoint = ( TempDepZnLd( ZoneNum ) * ( TempZoneThermostatSetPoint( ZoneNum ) ) - TempIndZnLd( ZoneNum ) );
    // LoadToCoolingSetPoint = ( TempDepZnLd( ZoneNum ) * ( TempZoneThermostatSetPoint( ZoneNum ) ) - TempIndZnLd( ZoneNum ) );
    int SetPointTempSchedIndex = state->dataZoneCtrls->TempControlledZone(HeatZoneNum).SchIndx_SingleHeatSetPoint;
    state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue = 20.0;
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand(HeatZoneNum).TotalOutputRequired = -1000.0; // cooling load
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(HeatZoneNum).tempDepLoad =
        state->dataZoneEnergyDemand->ZoneSysEnergyDemand(HeatZoneNum).TotalOutputRequired /
        state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue;

    CalcZoneAirTempSetPoints(*state);
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(HeatZoneNum).calcPredictedSystemLoad(*state, 1.0, HeatZoneNum);

    EXPECT_EQ(20.0, state->dataHeatBalFanSys->TempZoneThermostatSetPoint(HeatZoneNum));
    EXPECT_EQ(-1000.0,
              state->dataZoneEnergyDemand->ZoneSysEnergyDemand(HeatZoneNum)
                  .TotalOutputRequired); // TotalOutputRequired gets updated in CalcPredictedSystemLoad based on the load
    EXPECT_TRUE(state->dataZoneEnergyDemand->CurDeadBandOrSetback(HeatZoneNum)); // Tstat should show there is no load on a single heating SP

    SetPointTempSchedIndex = state->dataZoneCtrls->TempControlledZone(HeatZoneNum).SchIndx_SingleHeatSetPoint;
    state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue = 21.0;
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand(HeatZoneNum).TotalOutputRequired = 1000.0; // heating load
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(HeatZoneNum).tempDepLoad =
        state->dataZoneEnergyDemand->ZoneSysEnergyDemand(HeatZoneNum).TotalOutputRequired /
        state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue;

    SetPointTempSchedIndex = state->dataZoneCtrls->TempControlledZone(CoolZoneNum).SchIndx_SingleCoolSetPoint;
    state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue = 23.0;
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand(CoolZoneNum).TotalOutputRequired = -3000.0; // cooling load
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(CoolZoneNum).tempDepLoad =
        state->dataZoneEnergyDemand->ZoneSysEnergyDemand(CoolZoneNum).TotalOutputRequired /
        state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue;

    SetPointTempSchedIndex = state->dataZoneCtrls->TempControlledZone(CoolHeatZoneNum).SchIndx_SingleHeatCoolSetPoint;
    state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue = 22.0;
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand(CoolHeatZoneNum).TotalOutputRequired = -4000.0; // cooling load
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(CoolHeatZoneNum).tempDepLoad =
        state->dataZoneEnergyDemand->ZoneSysEnergyDemand(CoolHeatZoneNum).TotalOutputRequired /
        state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue;

    SetPointTempSchedIndex = state->dataZoneCtrls->TempControlledZone(DualZoneNum).SchIndx_DualSetPointWDeadBandCool;
    state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue = 24.0;
    SetPointTempSchedIndex = state->dataZoneCtrls->TempControlledZone(DualZoneNum).SchIndx_DualSetPointWDeadBandHeat;
    state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue = 20.0;
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand(DualZoneNum).TotalOutputRequired = 2500.0; // heating load
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(DualZoneNum).tempDepLoad =
        state->dataZoneEnergyDemand->ZoneSysEnergyDemand(DualZoneNum).TotalOutputRequired /
        state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue;

    CalcZoneAirTempSetPoints(*state);
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(HeatZoneNum).calcPredictedSystemLoad(*state, 1.0, HeatZoneNum);

    EXPECT_EQ(21.0, state->dataHeatBalFanSys->TempZoneThermostatSetPoint(HeatZoneNum));
    EXPECT_FALSE(state->dataZoneEnergyDemand->CurDeadBandOrSetback(HeatZoneNum)); // Tstat should show there is load on a single heating SP
    EXPECT_EQ(1000.0,
              state->dataZoneEnergyDemand->ZoneSysEnergyDemand(HeatZoneNum)
                  .TotalOutputRequired); // TotalOutputRequired gets updated in CalcPredictedSystemLoad based on the load

    state->dataZoneTempPredictorCorrector->zoneHeatBalance(CoolZoneNum).calcPredictedSystemLoad(*state, 1.0, CoolZoneNum);

    EXPECT_EQ(23.0, state->dataHeatBalFanSys->TempZoneThermostatSetPoint(CoolZoneNum));
    EXPECT_FALSE(state->dataZoneEnergyDemand->CurDeadBandOrSetback(CoolZoneNum)); // Tstat should show there is load on a single cooling SP
    EXPECT_EQ(-3000.0,
              state->dataZoneEnergyDemand->ZoneSysEnergyDemand(CoolZoneNum)
                  .TotalOutputRequired); // TotalOutputRequired gets updated in CalcPredictedSystemLoad based on the load

    state->dataZoneTempPredictorCorrector->zoneHeatBalance(CoolHeatZoneNum).calcPredictedSystemLoad(*state, 1.0, CoolHeatZoneNum);

    ASSERT_EQ(22.0, state->dataHeatBalFanSys->TempZoneThermostatSetPoint(CoolHeatZoneNum));
    EXPECT_FALSE(
        state->dataZoneEnergyDemand->CurDeadBandOrSetback(CoolHeatZoneNum)); // Tstat should show there is load on a single heating or cooling SP
    EXPECT_EQ(-4000.0,
              state->dataZoneEnergyDemand->ZoneSysEnergyDemand(CoolHeatZoneNum)
                  .TotalOutputRequired); // TotalOutputRequired gets updated in CalcPredictedSystemLoad based on the load

    state->dataZoneTempPredictorCorrector->zoneHeatBalance(DualZoneNum).calcPredictedSystemLoad(*state, 1.0, DualZoneNum);

    EXPECT_EQ(20.0, state->dataHeatBalFanSys->TempZoneThermostatSetPoint(DualZoneNum));
    EXPECT_FALSE(state->dataZoneEnergyDemand->CurDeadBandOrSetback(DualZoneNum)); // Tstat should show there is load on a dual SP
    EXPECT_EQ(2500.0,
              state->dataZoneEnergyDemand->ZoneSysEnergyDemand(DualZoneNum)
                  .TotalOutputRequired); // TotalOutputRequired gets updated in CalcPredictedSystemLoad based on the load

    SetPointTempSchedIndex = state->dataZoneCtrls->TempControlledZone(DualZoneNum).SchIndx_DualSetPointWDeadBandCool;
    state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue = 25.0;
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand(DualZoneNum).TotalOutputRequired = 1000.0;
    // LoadToCoolingSetPoint = ( TempDepZnLd( ZoneNum ) * ( TempZoneThermostatSetPoint( ZoneNum ) ) - TempIndZnLd( ZoneNum ) );
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(DualZoneNum).tempDepLoad =
        state->dataZoneEnergyDemand->ZoneSysEnergyDemand(DualZoneNum).TotalOutputRequired /
        state->dataScheduleMgr->Schedule(SetPointTempSchedIndex).CurrentValue;
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(DualZoneNum).tempIndLoad = 3500.0; // results in a cooling load

    CalcZoneAirTempSetPoints(*state);
    state->dataZoneTempPredictorCorrector->zoneHeatBalance(DualZoneNum).calcPredictedSystemLoad(*state, 1.0, DualZoneNum);

    EXPECT_EQ(25.0, state->dataHeatBalFanSys->TempZoneThermostatSetPoint(DualZoneNum));
    EXPECT_FALSE(state->dataZoneEnergyDemand->CurDeadBandOrSetback(DualZoneNum)); // Tstat should show there is load on a dual SP
    EXPECT_EQ(-2500.0, state->dataZoneEnergyDemand->ZoneSysEnergyDemand(DualZoneNum).TotalOutputRequired); // should show a cooling load
}

TEST_F(EnergyPlusFixture, ZoneTempPredictorCorrector_AdaptiveThermostat)
{
    // AUTHOR: Xuan Luo
    // DATE WRITTEN: Jan 2017

    std::string const idf_objects = delimited_string({
        "Zone,",
        "  Core_top,                !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "Zone,",
        "  Core_middle,             !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "Zone,",
        "  Core_basement,             !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "Zone,",
        "  Core_bottom,             !- Name",
        "  0.0000,                  !- Direction of Relative North {deg}",
        "  0.0000,                  !- X Origin {m}",
        "  0.0000,                  !- Y Origin {m}",
        "  0.0000,                  !- Z Origin {m}",
        "  1,                       !- Type",
        "  1,                       !- Multiplier",
        "  ,                        !- Ceiling Height {m}",
        "  ,                        !- Volume {m3}",
        "  autocalculate,           !- Floor Area {m2}",
        "  ,                        !- Zone Inside Convection Algorithm",
        "  ,                        !- Zone Outside Convection Algorithm",
        "  Yes;                     !- Part of Total Floor Area",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_top Thermostat,                   !- Name",
        "  Core_top,                              !- Zone or ZoneList Name",
        "  Single Cooling Control Type Sched,     !- Control Type Schedule Name",
        "  ThermostatSetpoint:SingleCooling,      !- Control 1 Object Type",
        "  Core_top CoolSPSched;                  !- Control 1 Name",
        " ",
        "ZoneControl:Thermostat:OperativeTemperature,",
        "  Core_top Thermostat,                   !- Thermostat Name",
        "  CONSTANT,                              !- Radiative Fraction Input Mode",
        "  0.0,                                   !- Fixed Radiative Fraction",
        "  ,                                      !- Radiative Fraction Schedule Name",
        "  AdaptiveASH55CentralLine;              !- Adaptive Comfort Model Type",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_middle Thermostat,                !- Name",
        "  Core_middle,                           !- Zone or ZoneList Name",
        "  Single Cooling Control Type Sched,     !- Control Type Schedule Name",
        "  ThermostatSetpoint:SingleCooling,      !- Control 1 Object Type",
        "  Core_middle CoolSPSched;               !- Control 1 Name",
        " ",
        "ZoneControl:Thermostat:OperativeTemperature,",
        "  Core_middle Thermostat,                !- Thermostat Name",
        "  CONSTANT,                              !- Radiative Fraction Input Mode",
        "  0.0,                                   !- Fixed Radiative Fraction",
        "  ,                                      !- Radiative Fraction Schedule Name",
        "  AdaptiveCEN15251CentralLine;           !- Adaptive Comfort Model Type",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_basement Thermostat,                   !- Name",
        "  Core_basement,                              !- Zone or ZoneList Name",
        "  Single Cooling Heating Control Type Sched,  !- Control Type Schedule Name",
        "  ThermostatSetpoint:SingleHeatingOrCooling,  !- Control 1 Object Type",
        "  Core_basement CoolHeatSPSched;              !- Control 1 Name",
        " ",
        "ZoneControl:Thermostat:OperativeTemperature,",
        "  Core_basement Thermostat,              !- Thermostat Name",
        "  CONSTANT,                              !- Radiative Fraction Input Mode",
        "  0.0,                                   !- Fixed Radiative Fraction",
        "  ,                                      !- Radiative Fraction Schedule Name",
        "  None;                                  !- Adaptive Comfort Model Type",
        " ",
        "ZoneControl:Thermostat,",
        "  Core_bottom Thermostat,                !- Name",
        "  Core_bottom,                           !- Zone or ZoneList Name",
        "  Dual Zone Control Type Sched,          !- Control Type Schedule Name",
        "  ThermostatSetpoint:DualSetpoint,       !- Control 1 Object Type",
        "  Core_bottom DualSPSched;               !- Control 1 Name",
        " ",
        "ZoneControl:Thermostat:OperativeTemperature,",
        "  Core_bottom Thermostat,                !- Thermostat Name",
        "  CONSTANT,                              !- Radiative Fraction Input Mode",
        "  0.0,                                   !- Fixed Radiative Fraction",
        "  ,                                      !- Radiative Fraction Schedule Name",
        "  AdaptiveASH55CentralLine;              !- Adaptive Comfort Model Type",
        " ",
        "ThermostatSetpoint:SingleCooling,",
        "  Core_middle CoolSPSched,               !- Name",
        "  SNGL_CLGSETP_SCH;                      !- Cooling Setpoint Temperature Schedule Name",
        " ",
        "ThermostatSetpoint:SingleHeatingOrCooling,",
        "  Core_basement CoolHeatSPSched,         !- Name",
        "  CLGHTGSETP_SCH;                        !- Heating Setpoint Temperature Schedule Name",
        " ",
        "ThermostatSetpoint:DualSetpoint,",
        "  Core_bottom DualSPSched,               !- Name",
        "  HTGSETP_SCH,                           !- Heating Setpoint Temperature Schedule Name",
        "  CLGSETP_SCH;                           !- Cooling Setpoint Temperature Schedule Name",
        " ",
        "Schedule:Compact,",
        "  Single Cooling Control Type Sched,  !- Name",
        "  Control Type,                          !- Schedule Type Limits Name",
        "  Through: 12/31,                        !- Field 1",
        "  For: AllDays,                          !- Field 2",
        "  Until: 24:00,2;                        !- Field 3",
        " ",
        "Schedule:Compact,",
        "  SNGL_CLGSETP_SCH,                      !- Name",
        "  Temperature,                           !- Schedule Type Limits Name",
        "  Through: 12/31,                        !- Field 1",
        "  For: AllDays,                          !- Field 2",
        "  Until: 24:00,24.0;                     !- Field 3",
        " ",
        "Schedule:Compact,",
        "  Single Cooling Heating Control Type Sched,  !- Name",
        "  Control Type,                          !- Schedule Type Limits Name",
        "  Through: 12/31,                        !- Field 1",
        "  For: AllDays,                          !- Field 2",
        "  Until: 24:00,3;                        !- Field 3",
        " ",
        "Schedule:Compact,",
        "  Dual Zone Control Type Sched,          !- Name",
        "  Control Type,                          !- Schedule Type Limits Name",
        "  Through: 12/31,                        !- Field 1",
        "  For: AllDays,                          !- Field 2",
        "  Until: 24:00,4;                        !- Field 3",
        " ",
        "Schedule:Compact,",
        "  CLGSETP_SCH,                           !- Name",
        "  Temperature,                           !- Schedule Type Limits Name",
        "  Through: 12/31,                        !- Field 1",
        "  For: AllDays,                          !- Field 2",
        "  Until: 24:00,24.0;                     !- Field 3",
        " ",
        "Schedule:Compact,",
        "  HTGSETP_SCH,                           !- Name",
        "  Temperature,                           !- Schedule Type Limits Name",
        "  Through: 12/31,                        !- Field 1",
        "  For: AllDays,                          !- Field 2",
        "  Until: 24:00,15.0;                     !- Field 3",
        " ",
        "Schedule:Compact,",
        "  CLGHTGSETP_SCH,                        !- Name",
        "  Temperature,                           !- Schedule Type Limits Name",
        "  Through: 12/31,                        !- Field 1",
        "  For: AllDays,                          !- Field 2",
        "  Until: 24:00,24.0;                     !- Field 3",
    });

    ASSERT_TRUE(process_idf(idf_objects)); // Tstat should show if the idf is legel

    int ZoneNum(4);
    int CoolZoneASHNum(1);
    int CoolZoneCENNum(2);
    int NoneAdapZoneNum(3);
    int DualZoneNum(4);
    int summerDesignDayTypeIndex(9);
    int constexpr ASH55_CENTRAL(2);
    int constexpr CEN15251_CENTRAL(5);

    state->dataEnvrn->DayOfYear = 1;
    state->dataWeather->Envrn = 1;
    state->dataWeather->Environment.allocate(1);
    state->dataWeather->DesDayInput.allocate(1);
    state->dataWeather->Environment(state->dataWeather->Envrn).KindOfEnvrn = Constant::KindOfSim::RunPeriodWeather;
    state->dataWeather->DesDayInput(state->dataWeather->Envrn).DayType = summerDesignDayTypeIndex;
    state->dataWeather->DesDayInput(state->dataWeather->Envrn).MaxDryBulb = 30.0;
    state->dataWeather->DesDayInput(state->dataWeather->Envrn).DailyDBRange = 10.0;
    Real64 ZoneAirSetPoint = 0.0;

    bool ErrorsFound(false); // If errors detected in input
    GetZoneData(*state, ErrorsFound);
    ASSERT_FALSE(ErrorsFound); // Tstat should show if there is error in zone processing
    ASSERT_FALSE(state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule
                     .initialized); // Tstat should show there adaptive model is not initialized

    Array1D<Real64> runningAverageASH_1(365, 0.0);
    Array1D<Real64> runningAverageCEN_1(365, 0.0);
    CalculateAdaptiveComfortSetPointSchl(*state, runningAverageASH_1, runningAverageCEN_1);
    // Tstat should show flage that adaptive comfort is not applicable (-1)
    ASSERT_EQ(
        -1, state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Central(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Upper_90(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Upper_80(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Central(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_I(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_II(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(-1,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_III(
                  state->dataEnvrn->DayOfYear));

    Array1D<Real64> runningAverageASH_2(365, 40.0);
    Array1D<Real64> runningAverageCEN_2(365, 40.0);
    CalculateAdaptiveComfortSetPointSchl(*state, runningAverageASH_2, runningAverageCEN_2);
    // Tstat should show flage that adaptive comfort is not applicable (-1)
    ASSERT_EQ(
        -1, state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Central(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Upper_90(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Upper_80(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Central(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_I(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(
        -1,
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_II(state->dataEnvrn->DayOfYear));
    ASSERT_EQ(-1,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_III(
                  state->dataEnvrn->DayOfYear));

    Array1D<Real64> runningAverageASH(365, 25.0);
    Array1D<Real64> runningAverageCEN(365, 25.0);
    CalculateAdaptiveComfortSetPointSchl(*state, runningAverageASH, runningAverageCEN);
    ASSERT_TRUE(
        state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.initialized); // Tstat should show there adaptive model is initialized
    ASSERT_EQ(25.55,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Central(
                  state->dataEnvrn->DayOfYear)); // Tstat should show ASH 55 CENTRAL LINE model set point
    ASSERT_EQ(28.05,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Upper_90(
                  state->dataEnvrn->DayOfYear)); // Tstat should show ASH 55 Upper 90 LINE model set point
    ASSERT_EQ(29.05,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Upper_80(
                  state->dataEnvrn->DayOfYear)); // Tstat should show ASH 55 Upper 80 LINE model set point
    ASSERT_EQ(27.05,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Central(
                  state->dataEnvrn->DayOfYear)); // Tstat should show CEN 15251 CENTRAL LINE model set point
    ASSERT_EQ(29.05,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_I(
                  state->dataEnvrn->DayOfYear)); // Tstat should show CEN 15251 Upper I LINE model set point
    ASSERT_EQ(30.05,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_II(
                  state->dataEnvrn->DayOfYear)); // Tstat should show CEN 15251 Upper II LINE model set point
    ASSERT_EQ(31.05,
              state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveCEN15251_Upper_III(
                  state->dataEnvrn->DayOfYear)); // Tstat should show CEN 15251 Upper III LINE model set point
    ASSERT_EQ(25.55,
              state->dataZoneTempPredictorCorrector->AdapComfortSetPointSummerDesDay[0]); // Tstat should show ASH 55 CENTRAL LINE model set point
    ASSERT_EQ(27.05,
              state->dataZoneTempPredictorCorrector->AdapComfortSetPointSummerDesDay[3]); // Tstat should show CEN 15251 CENTRAL LINE model set point

    state->dataZoneCtrls->TempControlledZone.allocate(ZoneNum);
    state->dataZoneCtrls->TempControlledZone(CoolZoneASHNum).AdaptiveComfortTempControl = true;
    state->dataZoneCtrls->TempControlledZone(CoolZoneASHNum).AdaptiveComfortModelTypeIndex = ASH55_CENTRAL;
    state->dataZoneCtrls->TempControlledZone(CoolZoneCENNum).AdaptiveComfortTempControl = true;
    state->dataZoneCtrls->TempControlledZone(CoolZoneCENNum).AdaptiveComfortModelTypeIndex = CEN15251_CENTRAL;
    state->dataZoneCtrls->TempControlledZone(NoneAdapZoneNum).AdaptiveComfortTempControl = true;
    state->dataZoneCtrls->TempControlledZone(NoneAdapZoneNum).AdaptiveComfortModelTypeIndex = ASH55_CENTRAL;
    state->dataZoneCtrls->TempControlledZone(DualZoneNum).AdaptiveComfortTempControl = true;
    state->dataZoneCtrls->TempControlledZone(DualZoneNum).AdaptiveComfortModelTypeIndex = ASH55_CENTRAL;

    ZoneAirSetPoint = 0.0;
    AdjustOperativeSetPointsforAdapComfort(*state, CoolZoneASHNum, ZoneAirSetPoint);
    ASSERT_EQ(25.55, ZoneAirSetPoint); // Tstat should show set point overwritten by ASH 55 CENTRAL LINE model

    ZoneAirSetPoint = 0.0;
    AdjustOperativeSetPointsforAdapComfort(*state, CoolZoneCENNum, ZoneAirSetPoint);
    ASSERT_EQ(27.05, ZoneAirSetPoint); // Tstat should show set point overwritten by CEN 15251 CENTRAL LINE model

    ZoneAirSetPoint = 0.0;
    state->dataZoneTempPredictorCorrector->AdapComfortDailySetPointSchedule.ThermalComfortAdaptiveASH55_Central(state->dataEnvrn->DayOfYear) = -1;
    AdjustOperativeSetPointsforAdapComfort(*state, NoneAdapZoneNum, ZoneAirSetPoint);
    ASSERT_EQ(0, ZoneAirSetPoint); // Tstat should show set point is not overwritten

    ZoneAirSetPoint = 26.0;
    AdjustOperativeSetPointsforAdapComfort(*state, DualZoneNum, ZoneAirSetPoint);
    ASSERT_EQ(26.0, ZoneAirSetPoint); // Tstat should show set point is not overwritten
}

TEST_F(EnergyPlusFixture, ZoneTempPredictorCorrector_calcZoneOrSpaceSums_SurfConvectionTest)
{
    // AUTHOR: L. Gu, FSEC
    // DATE WRITTEN: Jan 2017
    // #5906 Adaptive convection resulting in extremely low zone temperature which causes fatal error

    int ZoneNum = 1; // Zone number

    state->dataHeatBal->ZoneIntGain.allocate(ZoneNum);
    state->dataHeatBalFanSys->SumConvHTRadSys.allocate(ZoneNum);
    state->dataHeatBalFanSys->SumConvPool.allocate(ZoneNum);

    state->dataHeatBalFanSys->SumConvHTRadSys(1) = 0.0;
    state->dataHeatBalFanSys->SumConvPool(1) = 0.0;

    state->dataZoneEquip->ZoneEquipConfig.allocate(1);
    state->dataZoneEquip->ZoneEquipConfig(1).ZoneName = "Zone 1";

    state->dataZoneEquip->ZoneEquipConfig(1).NumInletNodes = 2;
    state->dataZoneEquip->ZoneEquipConfig(1).InletNode.allocate(2);
    state->dataZoneEquip->ZoneEquipConfig(1).InletNode(1) = 1;
    state->dataZoneEquip->ZoneEquipConfig(1).InletNode(2) = 2;
    state->dataZoneEquip->ZoneEquipConfig(1).NumExhaustNodes = 1;
    state->dataZoneEquip->ZoneEquipConfig(1).ExhaustNode.allocate(1);
    state->dataZoneEquip->ZoneEquipConfig(1).ExhaustNode(1) = 3;
    state->dataZoneEquip->ZoneEquipConfig(1).NumReturnNodes = 1;
    state->dataZoneEquip->ZoneEquipConfig(1).ReturnNode.allocate(1);
    state->dataZoneEquip->ZoneEquipConfig(1).ReturnNode(1) = 4;
    state->dataZoneEquip->ZoneEquipConfig(1).FixedReturnFlow.allocate(1);

    state->dataHeatBal->Zone.allocate(1);
    state->dataHeatBal->Zone(1).Name = state->dataZoneEquip->ZoneEquipConfig(1).ZoneName;
    state->dataHeatBal->Zone(1).IsControlled = true;
    state->dataSize->ZoneEqSizing.allocate(1);
    state->dataSize->CurZoneEqNum = 1;
    state->dataHeatBal->Zone(1).Multiplier = 1.0;
    state->dataHeatBal->Zone(1).Volume = 1000.0;
    state->dataHeatBal->Zone(1).SystemZoneNodeNumber = 5;
    state->dataHeatBal->Zone(1).ZoneVolCapMultpMoist = 1.0;
    state->dataHeatBalFanSys->SumLatentHTRadSys.allocate(1);
    state->dataHeatBalFanSys->SumLatentHTRadSys(1) = 0.0;
    state->dataHeatBalFanSys->SumLatentPool.allocate(1);
    state->dataHeatBalFanSys->SumLatentPool(1) = 0.0;
    state->dataEnvrn->OutBaroPress = 101325.0;
    state->dataZoneTempPredictorCorrector->zoneHeatBalance.allocate(1);
    state->dataZoneTempPredictorCorrector->spaceHeatBalance.allocate(1);
    auto &thisZoneHB = state->dataZoneTempPredictorCorrector->zoneHeatBalance(ZoneNum);
    thisZoneHB.MAT = 24.0;
    thisZoneHB.airHumRat = 0.001;

    state->dataHeatBal->space.allocate(1);
    state->dataHeatBal->spaceIntGainDevices.allocate(1);
    state->dataHeatBal->Zone(1).spaceIndexes.emplace_back(1);
    state->dataHeatBal->space(1).HTSurfaceFirst = 1;
    state->dataHeatBal->space(1).HTSurfaceLast = 3;
    state->dataSurface->Surface.allocate(3);
    state->dataHeatBalSurf->SurfHConvInt.allocate(3);
    state->dataLoopNodes->Node.allocate(4);
    state->dataHeatBal->SurfTempEffBulkAir.allocate(3);
    state->dataHeatBalSurf->SurfTempInTmp.allocate(3);

    state->dataSurface->SurfTAirRef.allocate(3);
    state->dataSurface->SurfTAirRef(1) = DataSurfaces::RefAirTemp::ZoneMeanAirTemp;
    state->dataSurface->SurfTAirRef(2) = DataSurfaces::RefAirTemp::AdjacentAirTemp;
    state->dataSurface->SurfTAirRef(3) = DataSurfaces::RefAirTemp::ZoneSupplyAirTemp;

    state->dataSurface->Surface(1).HeatTransSurf = true;
    state->dataSurface->Surface(2).HeatTransSurf = true;
    state->dataSurface->Surface(3).HeatTransSurf = true;
    state->dataSurface->Surface(1).Area = 10.0;
    state->dataSurface->Surface(2).Area = 10.0;
    state->dataSurface->Surface(3).Area = 10.0;
    state->dataHeatBalSurf->SurfTempInTmp(1) = 15.0;
    state->dataHeatBalSurf->SurfTempInTmp(2) = 20.0;
    state->dataHeatBalSurf->SurfTempInTmp(3) = 25.0;
    state->dataHeatBal->SurfTempEffBulkAir(1) = 10.0;
    state->dataHeatBal->SurfTempEffBulkAir(2) = 10.0;
    state->dataHeatBal->SurfTempEffBulkAir(3) = 10.0;

    state->dataLoopNodes->Node(1).Temp = 20.0;
    state->dataLoopNodes->Node(2).Temp = 20.0;
    state->dataLoopNodes->Node(3).Temp = 20.0;
    state->dataLoopNodes->Node(4).Temp = 20.0;
    state->dataLoopNodes->Node(1).MassFlowRate = 0.1;
    state->dataLoopNodes->Node(2).MassFlowRate = 0.1;
    state->dataLoopNodes->Node(3).MassFlowRate = 0.1;
    state->dataLoopNodes->Node(4).MassFlowRate = 0.1;

    state->dataHeatBalSurf->SurfHConvInt(1) = 0.5;
    state->dataHeatBalSurf->SurfHConvInt(2) = 0.5;
    state->dataHeatBalSurf->SurfHConvInt(3) = 0.5;

    state->dataZonePlenum->NumZoneReturnPlenums = 0;
    state->dataZonePlenum->NumZoneSupplyPlenums = 0;

    thisZoneHB.calcZoneOrSpaceSums(*state, true, ZoneNum);

    EXPECT_EQ(5.0, thisZoneHB.SumHA);
    EXPECT_EQ(300.0, thisZoneHB.SumHATsurf);
    EXPECT_EQ(150.0, thisZoneHB.SumHATref);

    state->dataLoopNodes->Node(1).MassFlowRate = 0.0;
    state->dataLoopNodes->Node(2).MassFlowRate = 0.0;
    thisZoneHB.calcZoneOrSpaceSums(*state, true, ZoneNum);
    EXPECT_EQ(10.0, thisZoneHB.SumHA);
    EXPECT_EQ(300.0, thisZoneHB.SumHATsurf);
    EXPECT_EQ(50.0, thisZoneHB.SumHATref);

    state->dataLoopNodes->Node(1).MassFlowRate = 0.1;
    state->dataLoopNodes->Node(2).MassFlowRate = 0.2;
    thisZoneHB.calcZoneOrSpaceSums(*state, true, ZoneNum);
    EXPECT_NEAR(302.00968500, thisZoneHB.SumSysMCp, 0.0001);
    EXPECT_NEAR(6040.1937, thisZoneHB.SumSysMCpT, 0.0001);

    thisZoneHB.calcZoneOrSpaceSums(*state, false, ZoneNum);
    EXPECT_EQ(0.0, thisZoneHB.SumSysMCp);
    EXPECT_EQ(0.0, thisZoneHB.SumSysMCpT);
}

TEST_F(EnergyPlusFixture, ZoneTempPredictorCorrector_EMSOverrideSetpointTest)
{
    // AUTHOR: L. Gu, FSEC
    // DATE WRITTEN: Jun. 2017
    // #5870 EMS actuators for Zone Temperature Control not working

    state->dataZoneCtrls->NumTempControlledZones = 1;
    state->dataZoneCtrls->NumComfortControlledZones = 0;
    state->dataZoneCtrls->TempControlledZone.allocate(1);
    state->dataZoneCtrls->TempControlledZone(1).EMSOverrideHeatingSetPointOn = true;
    state->dataZoneCtrls->TempControlledZone(1).EMSOverrideCoolingSetPointOn = true;
    state->dataZoneCtrls->TempControlledZone(1).ActualZoneNum = 1;
    state->dataZoneCtrls->TempControlledZone(1).EMSOverrideHeatingSetPointValue = 23;
    state->dataZoneCtrls->TempControlledZone(1).EMSOverrideCoolingSetPointValue = 26;

    state->dataHeatBalFanSys->TempControlType.allocate(1);
    state->dataHeatBalFanSys->TempControlTypeRpt.allocate(1);
    state->dataHeatBalFanSys->TempZoneThermostatSetPoint.allocate(1);
    state->dataHeatBalFanSys->ZoneThermostatSetPointLo.allocate(1);
    state->dataHeatBalFanSys->ZoneThermostatSetPointHi.allocate(1);
    state->dataHeatBalFanSys->TempControlType(1) = HVAC::ThermostatType::DualSetPointWithDeadBand;

    OverrideAirSetPointsforEMSCntrl(*state);
    EXPECT_EQ(23.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(26.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));

    state->dataZoneCtrls->NumTempControlledZones = 0;
    state->dataZoneCtrls->NumComfortControlledZones = 1;
    state->dataZoneCtrls->ComfortControlledZone.allocate(1);
    state->dataHeatBalFanSys->ComfortControlType.allocate(1);
    state->dataZoneCtrls->ComfortControlledZone(1).ActualZoneNum = 1;
    state->dataZoneCtrls->ComfortControlledZone(1).EMSOverrideHeatingSetPointOn = true;
    state->dataZoneCtrls->ComfortControlledZone(1).EMSOverrideCoolingSetPointOn = true;
    state->dataHeatBalFanSys->ComfortControlType(1) = HVAC::ThermostatType::DualSetPointWithDeadBand;
    state->dataZoneCtrls->ComfortControlledZone(1).EMSOverrideHeatingSetPointValue = 22;
    state->dataZoneCtrls->ComfortControlledZone(1).EMSOverrideCoolingSetPointValue = 25;

    OverrideAirSetPointsforEMSCntrl(*state);
    EXPECT_EQ(22.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(25.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));
}

TEST_F(EnergyPlusFixture, temperatureAndCountInSch_test)
{
    // J.Glazer - August 2017

    std::string const idf_objects = delimited_string({
        "ScheduleTypeLimits,",
        "  Any Number;              !- Name",
        " ",
        "Schedule:Compact,",
        " Sched1,                  !- Name",
        " Any Number,               !- Schedule Type Limits Name",
        " Through: 12/31,           !- Field 1",
        " For: AllDays,             !- Field 2",
        " Until: 24:00, 20.0;        !- Field 26",
        " ",
        "Schedule:Compact,",
        " Sched2,                  !- Name",
        " Any Number,               !- Schedule Type Limits Name",
        " Through: 1/31,            !- Field 1",
        " For: AllDays,             !- Field 2",
        " Until: 24:00, 24.0,        !- Field 26",
        " Through: 12/31,           !- Field 1",
        " For: AllDays,             !- Field 2",
        " Until: 24:00, 26.0;        !- Field 26",
        " ",
        "Schedule:Compact,",
        " Sched3,                  !- Name",
        " Any Number,               !- Schedule Type Limits Name",
        " Through: 1/31,            !- Field 1",
        " For: AllDays,             !- Field 2",
        " Until: 09:00, 24.0,        !- Field 26",
        " Until: 17:00, 26.0,        !- Field 26",
        " Until: 24:00, 24.0,        !- Field 26",
        " Through: 12/31,           !- Field 1",
        " For: AllDays,             !- Field 2",
        " Until: 24:00, 26.0;        !- Field 26",

    });

    ASSERT_TRUE(process_idf(idf_objects));

    state->dataGlobal->NumOfTimeStepInHour = 4;
    state->dataGlobal->MinutesPerTimeStep = 15;
    state->dataEnvrn->CurrentYearIsLeapYear = false;

    Real64 valueAtTime;
    int numDays;
    std::string monthAssumed;
    constexpr int wednesday = 4;

    state->dataEnvrn->Latitude = 30.; // northern hemisphere
    int sched1Index = GetScheduleIndex(*state, "SCHED1");
    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched1Index, false, wednesday, 11);

    EXPECT_EQ(20, valueAtTime);
    EXPECT_EQ(365, numDays);
    EXPECT_EQ("January", monthAssumed);

    // test month selected based on hemisphere and isSummer flag.
    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched1Index, true, wednesday, 11);
    EXPECT_EQ("July", monthAssumed);

    state->dataEnvrn->Latitude = -30.; // southern hemisphere
    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched1Index, false, wednesday, 11);
    EXPECT_EQ("July", monthAssumed);

    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched1Index, true, wednesday, 11);
    EXPECT_EQ("January", monthAssumed);

    state->dataEnvrn->Latitude = 30.; // northern hemisphere
    int sched2Index = GetScheduleIndex(*state, "SCHED2");
    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched2Index, false, wednesday, 11);

    EXPECT_EQ(24, valueAtTime);
    EXPECT_EQ(31, numDays);
    EXPECT_EQ("January", monthAssumed);

    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched2Index, true, wednesday, 11);

    EXPECT_EQ(26, valueAtTime);
    EXPECT_EQ(334, numDays);
    EXPECT_EQ("July", monthAssumed);

    int sched3Index = GetScheduleIndex(*state, "SCHED3");
    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched3Index, false, wednesday, 11);

    EXPECT_EQ(26, valueAtTime);
    EXPECT_EQ(365, numDays);
    EXPECT_EQ("January", monthAssumed);

    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched3Index, true, wednesday, 11);

    EXPECT_EQ(26, valueAtTime);
    EXPECT_EQ(365, numDays);
    EXPECT_EQ("July", monthAssumed);

    std::tie(valueAtTime, numDays, monthAssumed) = temperatureAndCountInSch(*state, sched3Index, false, wednesday, 19);

    EXPECT_EQ(24, valueAtTime);
    EXPECT_EQ(31, numDays);
    EXPECT_EQ("January", monthAssumed);
}

TEST_F(EnergyPlusFixture, SetPointWithCutoutDeltaT_test)
{
    // On/Off thermostat
    state->dataScheduleMgr->Schedule.allocate(3);

    state->dataZoneCtrls->NumTempControlledZones = 1;

    // SingleHeatingSetPoint
    state->dataZoneCtrls->TempControlledZone.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->TempZoneThermostatSetPoint.allocate(1);
    state->dataHeatBalFanSys->ZoneThermostatSetPointLo.allocate(1);
    state->dataHeatBalFanSys->ZoneThermostatSetPointHi.allocate(1);
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand.allocate(1);
    state->dataZoneEnergyDemand->DeadBandOrSetback.allocate(1);
    state->dataHeatBal->Zone.allocate(1);
    state->dataZoneEnergyDemand->Setback.allocate(1);

    state->dataZoneEnergyDemand->CurDeadBandOrSetback.allocate(1);
    state->dataHeatBalFanSys->LoadCorrectionFactor.allocate(1);
    state->dataZoneEnergyDemand->DeadBandOrSetback.allocate(1);

    state->dataHeatBal->ZoneAirSolutionAlgo = DataHeatBalance::SolutionAlgo::EulerMethod;

    state->dataZoneCtrls->TempControlledZone(1).DeltaTCutSet = 2.0;
    state->dataZoneCtrls->TempControlledZone(1).ActualZoneNum = 1;
    state->dataZoneCtrls->TempControlledZone(1).CTSchedIndex = 1;
    state->dataScheduleMgr->Schedule(1).CurrentValue = 1;
    state->dataHeatBalFanSys->TempControlType.allocate(1);
    state->dataHeatBalFanSys->TempControlTypeRpt.allocate(1);
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_SingleHeatSetPoint = 3;
    state->dataZoneTempPredictorCorrector->SetPointSingleHeating.allocate(1);
    state->dataZoneTempPredictorCorrector->SetPointSingleHeating(1).TempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 22.0;
    state->dataZoneTempPredictorCorrector->zoneHeatBalance.allocate(1);
    state->dataZoneTempPredictorCorrector->spaceHeatBalance.allocate(1);
    auto &thisZoneHB = state->dataZoneTempPredictorCorrector->zoneHeatBalance(1);
    thisZoneHB.AirPowerCap = 2000;
    thisZoneHB.tempDepLoad = 1.0;
    thisZoneHB.tempIndLoad = 1.0;

    thisZoneHB.MAT = 20.0;
    thisZoneHB.T1 = thisZoneHB.MAT;
    state->dataZoneTempPredictorCorrector->NumOnOffCtrZone = 1;

    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));

    thisZoneHB.MAT = 23.0;
    thisZoneHB.T1 = thisZoneHB.MAT;
    state->dataZoneCtrls->TempControlledZone(1).HeatModeLast = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(22.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    state->dataZoneCtrls->TempControlledZone(1).HeatModeLast = false;

    // SingleCoolingSetPoint
    state->dataScheduleMgr->Schedule(1).CurrentValue = 2;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_SingleCoolSetPoint = 3;
    state->dataZoneTempPredictorCorrector->SetPointSingleCooling.allocate(1);
    state->dataZoneTempPredictorCorrector->SetPointSingleCooling(1).TempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 26.0;
    thisZoneHB.MAT = 25.0;
    thisZoneHB.T1 = thisZoneHB.MAT;

    state->dataZoneCtrls->TempControlledZone(1).CoolModeLast = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(26.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));
    state->dataZoneCtrls->TempControlledZone(1).CoolModeLast = false;

    thisZoneHB.MAT = 27.0;
    thisZoneHB.T1 = thisZoneHB.MAT;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));

    // SingleHeatCoolSetPoint
    state->dataScheduleMgr->Schedule(1).CurrentValue = 3;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_SingleHeatCoolSetPoint = 3;
    state->dataZoneTempPredictorCorrector->SetPointSingleHeatCool.allocate(1);
    state->dataZoneTempPredictorCorrector->SetPointSingleHeatCool(1).TempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 24.0;
    thisZoneHB.MAT = 25.0;
    thisZoneHB.T1 = thisZoneHB.MAT;

    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));

    // DualSetPointWithDeadBand : Adjust cooling setpoint
    state->dataZoneTempPredictorCorrector->SetPointDualHeatCool.allocate(1);
    state->dataScheduleMgr->Schedule(1).CurrentValue = 4;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_DualSetPointWDeadBandHeat = 2;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_DualSetPointWDeadBandCool = 3;
    state->dataZoneTempPredictorCorrector->SetPointDualHeatCool(1).HeatTempSchedIndex = 2;
    state->dataZoneTempPredictorCorrector->SetPointDualHeatCool(1).CoolTempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(2).CurrentValue = 22.0;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 26.0;
    thisZoneHB.MAT = 25.0;
    thisZoneHB.T1 = thisZoneHB.MAT;

    state->dataZoneCtrls->TempControlledZone(1).CoolModeLast = true;
    state->dataZoneCtrls->TempControlledZone(1).HeatModeLast = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(22.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(26.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));
    state->dataZoneCtrls->TempControlledZone(1).HeatModeLast = false;

    // DualSetPointWithDeadBand : Adjust heating setpoint
    thisZoneHB.MAT = 21.0;
    thisZoneHB.T1 = thisZoneHB.MAT;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(26.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));

    // DualSetPointWithDeadBand : Adjust cooling setpoint
    state->dataZoneCtrls->TempControlledZone(1).CoolModeLast = true;
    thisZoneHB.MAT = 27.0;
    thisZoneHB.T1 = thisZoneHB.MAT;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(22.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));
}

TEST_F(EnergyPlusFixture, TempAtPrevTimeStepWithCutoutDeltaT_test)
{
    state->dataScheduleMgr->Schedule.allocate(3);
    state->dataZoneCtrls->NumTempControlledZones = 1;

    // SingleHeatingSetPoint
    state->dataZoneCtrls->TempControlledZone.allocate(state->dataZoneCtrls->NumTempControlledZones);
    state->dataHeatBalFanSys->TempZoneThermostatSetPoint.allocate(1);
    state->dataHeatBalFanSys->ZoneThermostatSetPointLo.allocate(1);
    state->dataHeatBalFanSys->ZoneThermostatSetPointHi.allocate(1);
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand.allocate(1);
    state->dataZoneEnergyDemand->DeadBandOrSetback.allocate(1);
    state->dataHeatBal->Zone.allocate(1);
    state->dataZoneEnergyDemand->Setback.allocate(1);

    state->dataZoneEnergyDemand->CurDeadBandOrSetback.allocate(1);
    state->dataHeatBalFanSys->LoadCorrectionFactor.allocate(1);
    state->dataZoneEnergyDemand->DeadBandOrSetback.allocate(1);

    state->dataHeatBal->ZoneAirSolutionAlgo = DataHeatBalance::SolutionAlgo::ThirdOrder;

    state->dataZoneCtrls->TempControlledZone(1).DeltaTCutSet = 2.0;
    state->dataZoneCtrls->TempControlledZone(1).ActualZoneNum = 1;
    state->dataZoneCtrls->TempControlledZone(1).CTSchedIndex = 1;
    state->dataScheduleMgr->Schedule(1).CurrentValue = 1;
    state->dataHeatBalFanSys->TempControlType.allocate(1);
    state->dataHeatBalFanSys->TempControlTypeRpt.allocate(1);
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_SingleHeatSetPoint = 3;
    state->dataZoneTempPredictorCorrector->SetPointSingleHeating.allocate(1);
    state->dataZoneTempPredictorCorrector->SetPointSingleHeating(1).TempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 22.0;
    state->dataZoneTempPredictorCorrector->spaceHeatBalance.allocate(1);
    state->dataZoneTempPredictorCorrector->zoneHeatBalance.allocate(1);
    auto &thisZoneHB = state->dataZoneTempPredictorCorrector->zoneHeatBalance(1);
    thisZoneHB.AirPowerCap = 2000;
    thisZoneHB.tempDepLoad = 1.0;
    thisZoneHB.tempIndLoad = 1.0;

    thisZoneHB.MAT = 20.0;
    thisZoneHB.XMPT = 23.0;
    state->dataZoneTempPredictorCorrector->NumOnOffCtrZone = 1;

    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));

    state->dataZoneCtrls->TempControlledZone(1).HeatModeLastSave = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, true, false, 0.01);
    EXPECT_EQ(22.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));

    // SingleCoolingSetPoint
    state->dataScheduleMgr->Schedule(1).CurrentValue = 2;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_SingleCoolSetPoint = 3;
    state->dataZoneTempPredictorCorrector->SetPointSingleCooling.allocate(1);
    state->dataZoneTempPredictorCorrector->SetPointSingleCooling(1).TempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 26.0;
    thisZoneHB.MAT = 25.0;
    thisZoneHB.XMPT = 27;

    state->dataZoneCtrls->TempControlledZone(1).CoolModeLast = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(26.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));
    state->dataZoneCtrls->TempControlledZone(1).CoolModeLast = false;

    state->dataZoneCtrls->TempControlledZone(1).CoolModeLastSave = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, true, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));

    // SingleHeatCoolSetPoint
    state->dataScheduleMgr->Schedule(1).CurrentValue = 3;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_SingleHeatCoolSetPoint = 3;
    state->dataZoneTempPredictorCorrector->SetPointSingleHeatCool.allocate(1);
    state->dataZoneTempPredictorCorrector->SetPointSingleHeatCool(1).TempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 24.0;
    thisZoneHB.MAT = 25.0;
    thisZoneHB.XMPT = thisZoneHB.MAT;

    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));

    // DualSetPointWithDeadBand : Adjust cooling setpoint
    state->dataZoneTempPredictorCorrector->SetPointDualHeatCool.allocate(1);
    state->dataScheduleMgr->Schedule(1).CurrentValue = 4;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_DualSetPointWDeadBandHeat = 2;
    state->dataZoneCtrls->TempControlledZone(1).SchIndx_DualSetPointWDeadBandCool = 3;
    state->dataZoneTempPredictorCorrector->SetPointDualHeatCool(1).HeatTempSchedIndex = 2;
    state->dataZoneTempPredictorCorrector->SetPointDualHeatCool(1).CoolTempSchedIndex = 3;
    state->dataScheduleMgr->Schedule(2).CurrentValue = 22.0;
    state->dataScheduleMgr->Schedule(3).CurrentValue = 26.0;
    thisZoneHB.MAT = 25.0;
    thisZoneHB.XMPT = 21.0;

    state->dataZoneCtrls->TempControlledZone(1).CoolModeLast = true;
    state->dataZoneCtrls->TempControlledZone(1).HeatModeLast = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, false, false, 0.01);
    EXPECT_EQ(22.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(26.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));
    state->dataZoneCtrls->TempControlledZone(1).HeatModeLast = false;

    // DualSetPointWithDeadBand : Adjust heating setpoint
    state->dataZoneCtrls->TempControlledZone(1).HeatModeLastSave = true;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, true, false, 0.01);
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(26.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));

    // DualSetPointWithDeadBand : Adjust cooling setpoint
    state->dataZoneCtrls->TempControlledZone(1).CoolModeLastSave = true;
    thisZoneHB.XMPT = 27.0;
    CalcZoneAirTempSetPoints(*state);
    PredictSystemLoads(*state, true, false, 0.01);
    EXPECT_EQ(22.0, state->dataHeatBalFanSys->ZoneThermostatSetPointLo(1));
    EXPECT_EQ(24.0, state->dataHeatBalFanSys->ZoneThermostatSetPointHi(1));
}

TEST_F(EnergyPlusFixture, ReportMoistLoadsZoneMultiplier_Test)
{
    int zoneNum = 1;
    state->dataZoneEnergyDemand->ZoneSysMoistureDemand.allocate(zoneNum);
    auto &thisZoneSysMoistureDemand = state->dataZoneEnergyDemand->ZoneSysMoistureDemand(zoneNum);
    state->dataHeatBal->Zone.allocate(zoneNum);
    auto &thisZone = state->dataHeatBal->Zone(zoneNum);
    Real64 ExpectedResult;
    Real64 AcceptableTolerance = 0.00001;

    // Test 1: Zone Multipliers are all unity (1.0).  So, single zone loads should be the same as total loads
    Real64 totalOutputRequired = 1000.0;
    Real64 outputRequiredToHumidifyingSP = 2000.0;
    Real64 outputRequiredToDehumidifyingSP = 3000.0;
    thisZone.Multiplier = 1.0;
    thisZone.ListMultiplier = 1.0;
    thisZoneSysMoistureDemand.reportMoistLoadsZoneMultiplier(
        *state, zoneNum, totalOutputRequired, outputRequiredToHumidifyingSP, outputRequiredToDehumidifyingSP);
    EXPECT_NEAR(thisZoneSysMoistureDemand.TotalOutputRequired, thisZoneSysMoistureDemand.predictedRate, AcceptableTolerance);
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToHumidifyingSP, thisZoneSysMoistureDemand.predictedHumSPRate, AcceptableTolerance);
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToDehumidifyingSP, thisZoneSysMoistureDemand.predictedDehumSPRate, AcceptableTolerance);

    // Test 2a: Zone Multiplier (non-list) is greater than 1, list Zone Multiplier is still one
    thisZone.Multiplier = 7.0;
    thisZone.ListMultiplier = 1.0;
    thisZoneSysMoistureDemand.reportMoistLoadsZoneMultiplier(
        *state, zoneNum, totalOutputRequired, outputRequiredToHumidifyingSP, outputRequiredToDehumidifyingSP);
    ExpectedResult = 1000.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedRate, AcceptableTolerance);
    ExpectedResult = 2000.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedHumSPRate, AcceptableTolerance);
    ExpectedResult = 3000.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedDehumSPRate, AcceptableTolerance);
    ExpectedResult = 7000.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.TotalOutputRequired, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 14000.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToHumidifyingSP, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 21000.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToDehumidifyingSP, ExpectedResult, AcceptableTolerance);

    // Test 2b: list Zone Multiplier is greater than 1, non-list Zone Multiplier is one
    thisZone.Multiplier = 1.0;
    thisZone.ListMultiplier = 7.0;
    thisZoneSysMoistureDemand.reportMoistLoadsZoneMultiplier(
        *state, zoneNum, totalOutputRequired, outputRequiredToHumidifyingSP, outputRequiredToDehumidifyingSP);
    ExpectedResult = 1000.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedRate, AcceptableTolerance);
    ExpectedResult = 2000.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedHumSPRate, AcceptableTolerance);
    ExpectedResult = 3000.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedDehumSPRate, AcceptableTolerance);
    ExpectedResult = 7000.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.TotalOutputRequired, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 14000.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToHumidifyingSP, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 21000.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToDehumidifyingSP, ExpectedResult, AcceptableTolerance);

    // Test 3: both zone multipliers are greater than 1.0
    totalOutputRequired = 300.0;
    outputRequiredToHumidifyingSP = 150.0;
    outputRequiredToDehumidifyingSP = 100.0;
    thisZone.Multiplier = 2.0;
    thisZone.ListMultiplier = 3.0;
    thisZoneSysMoistureDemand.reportMoistLoadsZoneMultiplier(
        *state, zoneNum, totalOutputRequired, outputRequiredToHumidifyingSP, outputRequiredToDehumidifyingSP);
    ExpectedResult = 300.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedRate, AcceptableTolerance);
    ExpectedResult = 150.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedHumSPRate, AcceptableTolerance);
    ExpectedResult = 100.0;
    EXPECT_NEAR(ExpectedResult, thisZoneSysMoistureDemand.predictedDehumSPRate, AcceptableTolerance);
    ExpectedResult = 1800.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.TotalOutputRequired, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 900.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToHumidifyingSP, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 600.0;
    EXPECT_NEAR(thisZoneSysMoistureDemand.OutputRequiredToDehumidifyingSP, ExpectedResult, AcceptableTolerance);
}

TEST_F(EnergyPlusFixture, ReportSensibleLoadsZoneMultiplier_Test)
{
    int zoneNum = 1;
    state->dataZoneEnergyDemand->ZoneSysEnergyDemand.allocate(zoneNum);
    auto &thisZoneSysEnergyDemand = state->dataZoneEnergyDemand->ZoneSysEnergyDemand(zoneNum);
    Real64 &SingleZoneTotRate = thisZoneSysEnergyDemand.predictedRate;
    Real64 &SingleZoneHeatRate = thisZoneSysEnergyDemand.predictedHSPRate;
    Real64 &SingleZoneCoolRate = thisZoneSysEnergyDemand.predictedCSPRate;
    state->dataHeatBalFanSys->LoadCorrectionFactor.allocate(zoneNum);
    Real64 &CorrectionFactor = state->dataHeatBalFanSys->LoadCorrectionFactor(zoneNum);
    state->dataHeatBal->Zone.allocate(zoneNum);
    auto &thisZone = state->dataHeatBal->Zone(zoneNum);
    Real64 ExpectedResult;
    Real64 AcceptableTolerance = 0.00001;

    // Test 1: Zone Multipliers and Load Correction Factor are all unity (1.0).  So, single zone loads should be the same as total loads
    thisZoneSysEnergyDemand.TotalOutputRequired = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToHeatingSP = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToCoolingSP = 0.0;
    Real64 totalOutputRequired = 1000.0;
    Real64 HeatToSP = 2000.0;
    Real64 CoolToSP = 3000.0;
    CorrectionFactor = 1.0;
    thisZone.Multiplier = 1.0;
    thisZone.ListMultiplier = 1.0;
    thisZoneSysEnergyDemand.reportSensibleLoadsZoneMultiplier(*state, zoneNum, totalOutputRequired, HeatToSP, CoolToSP);
    EXPECT_NEAR(thisZoneSysEnergyDemand.TotalOutputRequired, SingleZoneTotRate, AcceptableTolerance);
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToHeatingSP, SingleZoneHeatRate, AcceptableTolerance);
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToCoolingSP, SingleZoneCoolRate, AcceptableTolerance);

    // Test 2a: Zone Multiplier (non-list) is greater than 1, list Zone Multiplier and Load Correction are still one
    thisZoneSysEnergyDemand.TotalOutputRequired = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToHeatingSP = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToCoolingSP = 0.0;
    totalOutputRequired = 1000.0;
    HeatToSP = 2000.0;
    CoolToSP = 3000.0;
    CorrectionFactor = 1.0;
    thisZone.Multiplier = 4.0;
    thisZone.ListMultiplier = 1.0;
    thisZoneSysEnergyDemand.reportSensibleLoadsZoneMultiplier(*state, zoneNum, totalOutputRequired, HeatToSP, CoolToSP);
    ExpectedResult = 1000.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneTotRate, AcceptableTolerance);
    ExpectedResult = 2000.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneHeatRate, AcceptableTolerance);
    ExpectedResult = 3000.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneCoolRate, AcceptableTolerance);
    ExpectedResult = 4000.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.TotalOutputRequired, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 8000.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToHeatingSP, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 12000.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToCoolingSP, ExpectedResult, AcceptableTolerance);

    // Test 2b: list Zone Multiplier is greater than 1, non-list Zone Multiplier and Load Correction are still one
    thisZoneSysEnergyDemand.TotalOutputRequired = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToHeatingSP = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToCoolingSP = 0.0;
    totalOutputRequired = 1000.0;
    HeatToSP = 2000.0;
    CoolToSP = 3000.0;
    CorrectionFactor = 1.0;
    thisZone.Multiplier = 1.0;
    thisZone.ListMultiplier = 5.0;
    thisZoneSysEnergyDemand.reportSensibleLoadsZoneMultiplier(*state, zoneNum, totalOutputRequired, HeatToSP, CoolToSP);
    ExpectedResult = 1000.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneTotRate, AcceptableTolerance);
    ExpectedResult = 2000.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneHeatRate, AcceptableTolerance);
    ExpectedResult = 3000.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneCoolRate, AcceptableTolerance);
    ExpectedResult = 5000.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.TotalOutputRequired, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 10000.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToHeatingSP, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 15000.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToCoolingSP, ExpectedResult, AcceptableTolerance);

    // Test 2c: list Zone Multiplier and Zone Multiplier are unity, Load Correction is not equal to 1.0
    thisZoneSysEnergyDemand.TotalOutputRequired = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToHeatingSP = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToCoolingSP = 0.0;
    totalOutputRequired = 1000.0;
    HeatToSP = 2000.0;
    CoolToSP = 3000.0;
    CorrectionFactor = 1.1;
    thisZone.Multiplier = 1.0;
    thisZone.ListMultiplier = 1.0;
    thisZoneSysEnergyDemand.reportSensibleLoadsZoneMultiplier(*state, zoneNum, totalOutputRequired, HeatToSP, CoolToSP);
    ExpectedResult = 1100.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneTotRate, AcceptableTolerance);
    ExpectedResult = 2200.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneHeatRate, AcceptableTolerance);
    ExpectedResult = 3300.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneCoolRate, AcceptableTolerance);
    ExpectedResult = 1100.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.TotalOutputRequired, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 2200.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToHeatingSP, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 3300.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToCoolingSP, ExpectedResult, AcceptableTolerance);

    // Test 3: none of the multipliers are unity
    thisZoneSysEnergyDemand.TotalOutputRequired = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToHeatingSP = 0.0;
    thisZoneSysEnergyDemand.OutputRequiredToCoolingSP = 0.0;
    totalOutputRequired = 1000.0;
    HeatToSP = 2000.0;
    CoolToSP = 3000.0;
    CorrectionFactor = 1.2;
    thisZone.Multiplier = 1.0;
    thisZone.ListMultiplier = 3.0;
    thisZoneSysEnergyDemand.reportSensibleLoadsZoneMultiplier(*state, zoneNum, totalOutputRequired, HeatToSP, CoolToSP);
    ExpectedResult = 1200.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneTotRate, AcceptableTolerance);
    ExpectedResult = 2400.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneHeatRate, AcceptableTolerance);
    ExpectedResult = 3600.0;
    EXPECT_NEAR(ExpectedResult, SingleZoneCoolRate, AcceptableTolerance);
    ExpectedResult = 3600.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.TotalOutputRequired, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 7200.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToHeatingSP, ExpectedResult, AcceptableTolerance);
    ExpectedResult = 10800.0;
    EXPECT_NEAR(thisZoneSysEnergyDemand.OutputRequiredToCoolingSP, ExpectedResult, AcceptableTolerance);
}

TEST_F(EnergyPlusFixture, DownInterpolate4HistoryValues_Test)
{
    Real64 PriorTimeStep = 0.25;
    state->dataHVACGlobal->TimeStepSys = 0.125;
    state->dataHVACGlobal->TimeStepSysSec = state->dataHVACGlobal->TimeStepSys * Constant::SecInHour;
    Real64 myVarValue = 5.0;
    Real64 HistoryValue1 = 1.0;
    Real64 HistoryValue2 = 2.0;
    Real64 HistoryValue3 = 3.0;
    Real64 DSHistoryValue1 = 0.0;
    Real64 DSHistoryValue2 = 0.0;
    Real64 DSHistoryValue3 = 0.0;
    Real64 DSHistoryValue4 = 0.0;

    EXPECT_NEAR(myVarValue, 5.0, 0.000001); // value after corrector and before simulation down-shifts
    DownInterpolate4HistoryValues(PriorTimeStep,
                                  state->dataHVACGlobal->TimeStepSys,
                                  HistoryValue1,
                                  HistoryValue2,
                                  HistoryValue3,
                                  myVarValue,
                                  DSHistoryValue1,
                                  DSHistoryValue2,
                                  DSHistoryValue3,
                                  DSHistoryValue4);
    EXPECT_NEAR(myVarValue, HistoryValue1, 0.000001); // setting up history terms for shortened time step simulation
    EXPECT_NEAR(DSHistoryValue1, 1.5, 0.000001);      // values are interpolated to provide history terms at the new time step
    EXPECT_NEAR(DSHistoryValue2, 2.0, 0.000001);
    EXPECT_NEAR(DSHistoryValue3, 2.5, 0.000001);
    EXPECT_NEAR(DSHistoryValue4, 3.0, 0.000001);

    std::array<Real64, 4> newValue = {0.0, 0.0, 0.0, 0.0};
    std::array<Real64, 4> oldValue = {DSHistoryValue1, DSHistoryValue2, DSHistoryValue3, DSHistoryValue4};
    Real64 returnValue = DownInterpolate4HistoryValues(PriorTimeStep, state->dataHVACGlobal->TimeStepSys, oldValue, newValue);
    EXPECT_NEAR(returnValue, oldValue[0], 0.000001); // setting up history terms for shortened time step simulation
    EXPECT_NEAR(newValue[0], 1.5, 0.000001);         // values are interpolated to provide history terms at the new time step
    EXPECT_NEAR(newValue[1], 1.75, 0.000001);
    EXPECT_NEAR(newValue[2], 2.0, 0.000001);
    EXPECT_NEAR(newValue[3], 2.25, 0.000001);
    EXPECT_NEAR(oldValue[0], DSHistoryValue1, 0.000001); // values are same as before
    EXPECT_NEAR(oldValue[1], DSHistoryValue2, 0.000001);
    EXPECT_NEAR(oldValue[2], DSHistoryValue3, 0.000001);
    EXPECT_NEAR(oldValue[3], DSHistoryValue4, 0.000001);
}

TEST_F(EnergyPlusFixture, HybridModel_processInverseModelMultpHMTest)
{
    // Test added for fix to GitHub Issue #10508
    Real64 calcHMmult;
    Real64 calcHMsum = 0.0;
    Real64 calcHMcount = 0.0;
    Real64 calcHMavg = 0.0;
    Real64 expectedHMmult;
    Real64 expectedHMsum;
    Real64 expectedHMcount;
    Real64 expectedHMavg;
    int numZones = 1;
    Real64 constexpr allowableTolerance = 0.001;

    state->dataHeatBal->Zone.allocate(numZones);
    state->dataHeatBal->Zone(numZones).Name = "Hybrid Zone";
    state->dataZoneTempPredictorCorrector->zoneHeatBalance.allocate(numZones);

    // Test 1: Multiplier is less than the minimum.  Reset to the minimum. Nothing added to averages.
    calcHMmult = 0.5;
    expectedHMmult = 1.0;
    expectedHMsum = 0.0;
    expectedHMcount = 0;
    expectedHMavg = 0.0;
    processInverseModelMultpHM(*state, calcHMmult, calcHMsum, calcHMcount, calcHMavg, numZones);
    EXPECT_NEAR(calcHMmult, expectedHMmult, allowableTolerance);
    EXPECT_NEAR(calcHMsum, expectedHMsum, allowableTolerance);
    EXPECT_NEAR(calcHMcount, expectedHMcount, allowableTolerance);
    EXPECT_NEAR(calcHMavg, expectedHMavg, allowableTolerance);
    EXPECT_EQ(state->dataZoneTempPredictorCorrector->zoneHeatBalance(numZones).hmThermalMassMultErrIndex, 0);

    // Test 2: Multiplier is equal to minimum.  Reset to the minimum. Nothing added to averages.
    calcHMmult = 1.0;
    expectedHMmult = 1.0;
    expectedHMsum = 0.0;
    expectedHMcount = 0;
    expectedHMavg = 0.0;
    processInverseModelMultpHM(*state, calcHMmult, calcHMsum, calcHMcount, calcHMavg, numZones);
    EXPECT_NEAR(calcHMmult, expectedHMmult, allowableTolerance);
    EXPECT_NEAR(calcHMsum, expectedHMsum, allowableTolerance);
    EXPECT_NEAR(calcHMcount, expectedHMcount, allowableTolerance);
    EXPECT_NEAR(calcHMavg, expectedHMavg, allowableTolerance);
    EXPECT_EQ(state->dataZoneTempPredictorCorrector->zoneHeatBalance(numZones).hmThermalMassMultErrIndex, 0);

    // Test 3: Multiplier is greater than minimum but less than maximum.  Set the statistical variables accordingly.
    calcHMmult = 10.0;
    expectedHMmult = 10.0;
    expectedHMsum = 10.0;
    expectedHMcount = 1;
    expectedHMavg = 10.0;
    processInverseModelMultpHM(*state, calcHMmult, calcHMsum, calcHMcount, calcHMavg, numZones);
    EXPECT_NEAR(calcHMmult, expectedHMmult, allowableTolerance);
    EXPECT_NEAR(calcHMsum, expectedHMsum, allowableTolerance);
    EXPECT_NEAR(calcHMcount, expectedHMcount, allowableTolerance);
    EXPECT_NEAR(calcHMavg, expectedHMavg, allowableTolerance);
    EXPECT_EQ(state->dataZoneTempPredictorCorrector->zoneHeatBalance(numZones).hmThermalMassMultErrIndex, 0);

    // Test 4: Multiplier is greater than maximum.  Produce an error message but still set the statistical variables accordingly.
    calcHMmult = 50.0;
    expectedHMmult = 50.0;
    expectedHMsum = 60.0;
    expectedHMcount = 2;
    expectedHMavg = 30.0;
    processInverseModelMultpHM(*state, calcHMmult, calcHMsum, calcHMcount, calcHMavg, numZones);
    EXPECT_NEAR(calcHMmult, expectedHMmult, allowableTolerance);
    EXPECT_NEAR(calcHMsum, expectedHMsum, allowableTolerance);
    EXPECT_NEAR(calcHMcount, expectedHMcount, allowableTolerance);
    EXPECT_NEAR(calcHMavg, expectedHMavg, allowableTolerance);
    EXPECT_NE(state->dataZoneTempPredictorCorrector->zoneHeatBalance(numZones).hmThermalMassMultErrIndex,
              0); // This is now set, won't be zero anymore
    std::string const error_string =
        delimited_string({"   ** Warning ** Hybrid model thermal mass multiplier higher than the limit for Hybrid Zone",
                          "   **   ~~~   ** This means that the ratio of the zone air heat capacity for the current time step to the",
                          "   **   ~~~   ** zone air heat storage is higher than the maximum limit of 30.0."});
    EXPECT_TRUE(compare_err_stream(error_string, true));

    // Test 5: Repeat of Test 1--verifying that it won't impact the statistical variables.  No error message.
    calcHMmult = 0.5;
    expectedHMmult = 1.0;
    expectedHMsum = 60.0;
    expectedHMcount = 2;
    expectedHMavg = 30.0;
    processInverseModelMultpHM(*state, calcHMmult, calcHMsum, calcHMcount, calcHMavg, numZones);
    EXPECT_NEAR(calcHMmult, expectedHMmult, allowableTolerance);
    EXPECT_NEAR(calcHMsum, expectedHMsum, allowableTolerance);
    EXPECT_NEAR(calcHMcount, expectedHMcount, allowableTolerance);
    EXPECT_NEAR(calcHMavg, expectedHMavg, allowableTolerance);
    EXPECT_NE(state->dataZoneTempPredictorCorrector->zoneHeatBalance(numZones).hmThermalMassMultErrIndex, 0);
}
