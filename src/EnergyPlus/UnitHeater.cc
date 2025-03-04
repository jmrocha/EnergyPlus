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

// C++ Headers
#include <cmath>

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Fmath.hh>

// EnergyPlus Headers
#include <EnergyPlus/Autosizing/HeatingAirFlowSizing.hh>
#include <EnergyPlus/Autosizing/HeatingCapacitySizing.hh>
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/Data/EnergyPlusData.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataHeatBalance.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/DataZoneEnergyDemands.hh>
#include <EnergyPlus/DataZoneEquipment.hh>
#include <EnergyPlus/Fans.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/GeneralRoutines.hh>
#include <EnergyPlus/HeatingCoils.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ReportCoilSelection.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/SteamCoils.hh>
#include <EnergyPlus/UnitHeater.hh>
#include <EnergyPlus/UtilityRoutines.hh>
#include <EnergyPlus/WaterCoils.hh>

namespace EnergyPlus {

namespace UnitHeater {

    // Module containing the routines dealing with the Unit Heater

    // MODULE INFORMATION:
    //       AUTHOR         Rick Strand
    //       DATE WRITTEN   May 2000
    //       MODIFIED       Brent Griffith, Sept 2010, plant upgrades, fluid properties
    //       MODIFIED       Bereket Nigusse, FSEC, October 2013, Added cycling fan operating mode
    //       RE-ENGINEERED  na

    // PURPOSE OF THIS MODULE:
    // To simulate unit heaters.  It is assumed that unit heaters are zone equipment
    // without any connection to outside air other than through a separately defined
    // air loop.

    // METHODOLOGY EMPLOYED:
    // Units are modeled as a collection of a fan and a heating coil.  The fan
    // can either be a continuously running fan or an on-off fan which turns on
    // only when there is actually a heating load.  This fan control works together
    // with the unit operation schedule to determine what the unit heater actually
    // does at a given point in time.

    // REFERENCES:
    // ASHRAE Systems and Equipment Handbook (SI), 1996. pp. 31.3-31.8
    // Rick Strand's unit heater module which was based upon Fred Buhl's fan coil
    // module (FanCoilUnits.cc)

    // Using/Aliasing
    using namespace DataLoopNode;
    using HVAC::SmallAirVolFlow;
    using HVAC::SmallLoad;
    using HVAC::SmallMassFlow;
    using namespace ScheduleManager;
    using Psychrometrics::PsyCpAirFnW;
    using Psychrometrics::PsyHFnTdbW;
    using Psychrometrics::PsyRhoAirFnPbTdbW;
    using namespace FluidProperties;

    static constexpr std::string_view fluidNameSteam("STEAM");

    void SimUnitHeater(EnergyPlusData &state,
                       std::string_view CompName,     // name of the fan coil unit
                       int const ZoneNum,             // number of zone being served
                       bool const FirstHVACIteration, // TRUE if 1st HVAC simulation of system timestep
                       Real64 &PowerMet,              // Sensible power supplied (W)
                       Real64 &LatOutputProvided,     // Latent add/removal supplied by window AC (kg/s), dehumid = negative
                       int &CompIndex)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   May 2000
        //       MODIFIED       Don Shirey, Aug 2009 (LatOutputProvided)
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This is the main driver subroutine for the Unit Heater simulation.

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int UnitHeatNum; // index of unit heater being simulated

        if (state.dataUnitHeaters->GetUnitHeaterInputFlag) {
            GetUnitHeaterInput(state);
            state.dataUnitHeaters->GetUnitHeaterInputFlag = false;
        }

        // Find the correct Unit Heater Equipment
        if (CompIndex == 0) {
            UnitHeatNum = Util::FindItemInList(CompName, state.dataUnitHeaters->UnitHeat);
            if (UnitHeatNum == 0) {
                ShowFatalError(state, format("SimUnitHeater: Unit not found={}", CompName));
            }
            CompIndex = UnitHeatNum;
        } else {
            UnitHeatNum = CompIndex;
            if (UnitHeatNum > state.dataUnitHeaters->NumOfUnitHeats || UnitHeatNum < 1) {
                ShowFatalError(state,
                               format("SimUnitHeater:  Invalid CompIndex passed={}, Number of Units={}, Entered Unit name={}",
                                      UnitHeatNum,
                                      state.dataUnitHeaters->NumOfUnitHeats,
                                      CompName));
            }
            if (state.dataUnitHeaters->CheckEquipName(UnitHeatNum)) {
                if (CompName != state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name) {
                    ShowFatalError(state,
                                   format("SimUnitHeater: Invalid CompIndex passed={}, Unit name={}, stored Unit Name for that index={}",
                                          UnitHeatNum,
                                          CompName,
                                          state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                }
                state.dataUnitHeaters->CheckEquipName(UnitHeatNum) = false;
            }
        }

        state.dataSize->ZoneEqUnitHeater = true;

        InitUnitHeater(state, UnitHeatNum, ZoneNum, FirstHVACIteration);

        state.dataSize->ZoneHeatingOnlyFan = true;

        CalcUnitHeater(state, UnitHeatNum, ZoneNum, FirstHVACIteration, PowerMet, LatOutputProvided);

        state.dataSize->ZoneHeatingOnlyFan = false;

        //  CALL UpdateUnitHeater

        ReportUnitHeater(state, UnitHeatNum);

        state.dataSize->ZoneEqUnitHeater = false;
    }

    void GetUnitHeaterInput(EnergyPlusData &state)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   May 2000
        //       MODIFIED       Chandan Sharma, FSEC, March 2011: Added ZoneHVAC sys avail manager
        //                      Bereket Nigusse, FSEC, April 2011: eliminated input node names
        //                                                         & added fan object type
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Obtain the user input data for all of the unit heaters in the input file.

        // METHODOLOGY EMPLOYED:
        // Standard EnergyPlus methodology.

        // REFERENCES:
        // Fred Buhl's fan coil module (FanCoilUnits.cc)

        // Using/Aliasing
        using BranchNodeConnections::SetUpCompSets;

        using DataSizing::AutoSize;
        using NodeInputManager::GetOnlySingleNode;
        using SteamCoils::GetCoilSteamInletNode;
        using SteamCoils::GetSteamCoilIndex;
        using WaterCoils::GetCoilWaterInletNode;

        static constexpr std::string_view RoutineName("GetUnitHeaterInput: "); // include trailing blank space
        static constexpr std::string_view routineName = "GetUnitHeaterInput";  // include trailing blank space

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        bool ErrorsFound(false); // Set to true if errors in input, fatal at end of routine
        int IOStatus;            // Used in GetObjectItem
        bool IsNotOK;            // TRUE if there was a problem with a list name
        bool errFlag(false);     // interim error flag
        int NumAlphas;           // Number of Alphas for each GetObjectItem call
        int NumNumbers;          // Number of Numbers for each GetObjectItem call
        int NumFields;           // Total number of fields in object
        int UnitHeatNum;         // Item to be "gotten"

        Real64 FanVolFlow; // Fan volumetric flow rate
        std::string CurrentModuleObject;
        Array1D_string Alphas;         // Alpha items for object
        Array1D<Real64> Numbers;       // Numeric items for object
        Array1D_string cAlphaFields;   // Alpha field names
        Array1D_string cNumericFields; // Numeric field names
        Array1D_bool lAlphaBlanks;     // Logical array, alpha field input BLANK = .TRUE.
        Array1D_bool lNumericBlanks;   // Logical array, numeric field input BLANK = .TRUE.
        int CtrlZone;                  // index to loop counter
        int NodeNum;                   // index to loop counter
        bool ZoneNodeNotFound;         // used in error checking

        // Figure out how many unit heaters there are in the input file
        CurrentModuleObject = state.dataUnitHeaters->cMO_UnitHeater;
        state.dataUnitHeaters->NumOfUnitHeats = state.dataInputProcessing->inputProcessor->getNumObjectsFound(state, CurrentModuleObject);
        state.dataInputProcessing->inputProcessor->getObjectDefMaxArgs(state, CurrentModuleObject, NumFields, NumAlphas, NumNumbers);

        Alphas.allocate(NumAlphas);
        Numbers.dimension(NumNumbers, 0.0);
        cAlphaFields.allocate(NumAlphas);
        cNumericFields.allocate(NumNumbers);
        lAlphaBlanks.dimension(NumAlphas, true);
        lNumericBlanks.dimension(NumNumbers, true);

        // Allocate the local derived type and do one-time initializations for all parts of it
        if (state.dataUnitHeaters->NumOfUnitHeats > 0) {
            state.dataUnitHeaters->UnitHeat.allocate(state.dataUnitHeaters->NumOfUnitHeats);
            state.dataUnitHeaters->CheckEquipName.allocate(state.dataUnitHeaters->NumOfUnitHeats);
            state.dataUnitHeaters->UnitHeatNumericFields.allocate(state.dataUnitHeaters->NumOfUnitHeats);
        }
        state.dataUnitHeaters->CheckEquipName = true;

        for (UnitHeatNum = 1; UnitHeatNum <= state.dataUnitHeaters->NumOfUnitHeats;
             ++UnitHeatNum) { // Begin looping over all of the unit heaters found in the input file...

            state.dataInputProcessing->inputProcessor->getObjectItem(state,
                                                                     CurrentModuleObject,
                                                                     UnitHeatNum,
                                                                     Alphas,
                                                                     NumAlphas,
                                                                     Numbers,
                                                                     NumNumbers,
                                                                     IOStatus,
                                                                     lNumericBlanks,
                                                                     lAlphaBlanks,
                                                                     cAlphaFields,
                                                                     cNumericFields);

            ErrorObjectHeader eoh{routineName, CurrentModuleObject, Alphas(1)};

            state.dataUnitHeaters->UnitHeatNumericFields(UnitHeatNum).FieldNames.allocate(NumNumbers);
            state.dataUnitHeaters->UnitHeatNumericFields(UnitHeatNum).FieldNames = "";
            state.dataUnitHeaters->UnitHeatNumericFields(UnitHeatNum).FieldNames = cNumericFields;
            Util::IsNameEmpty(state, Alphas(1), CurrentModuleObject, ErrorsFound);

            state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name = Alphas(1);
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).SchedName = Alphas(2);
            if (lAlphaBlanks(2)) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).SchedPtr = ScheduleManager::ScheduleAlwaysOn;
            } else {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).SchedPtr = GetScheduleIndex(state, Alphas(2)); // convert schedule name to pointer
                if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).SchedPtr == 0) {
                    ShowSevereError(state,
                                    format("{}{}: invalid {} entered ={} for {}={}",
                                           RoutineName,
                                           CurrentModuleObject,
                                           cAlphaFields(2),
                                           Alphas(2),
                                           cAlphaFields(1),
                                           Alphas(1)));
                    ErrorsFound = true;
                }
            }

            // Main air nodes (except outside air node):
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode = GetOnlySingleNode(state,
                                                                                       Alphas(3),
                                                                                       ErrorsFound,
                                                                                       DataLoopNode::ConnectionObjectType::ZoneHVACUnitHeater,
                                                                                       Alphas(1),
                                                                                       DataLoopNode::NodeFluidType::Air,
                                                                                       DataLoopNode::ConnectionType::Inlet,
                                                                                       NodeInputManager::CompFluidStream::Primary,
                                                                                       ObjectIsParent);

            state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode = GetOnlySingleNode(state,
                                                                                        Alphas(4),
                                                                                        ErrorsFound,
                                                                                        DataLoopNode::ConnectionObjectType::ZoneHVACUnitHeater,
                                                                                        Alphas(1),
                                                                                        DataLoopNode::NodeFluidType::Air,
                                                                                        DataLoopNode::ConnectionType::Outlet,
                                                                                        NodeInputManager::CompFluidStream::Primary,
                                                                                        ObjectIsParent);

            auto &unitHeat = state.dataUnitHeaters->UnitHeat(UnitHeatNum);
            // Fan information:
            unitHeat.fanType = static_cast<HVAC::FanType>(getEnumValue(HVAC::fanTypeNamesUC, Alphas(5)));
            if (unitHeat.fanType != HVAC::FanType::Constant && unitHeat.fanType != HVAC::FanType::VAV && unitHeat.fanType != HVAC::FanType::OnOff &&
                unitHeat.fanType != HVAC::FanType::SystemModel) {
                ShowSevereInvalidKey(state, eoh, cAlphaFields(5), Alphas(5), "Fan Type must be Fan:ConstantVolume, Fan:VariableVolume, or Fan:OnOff");
                ErrorsFound = true;
            }

            unitHeat.FanName = Alphas(6);
            unitHeat.MaxAirVolFlow = Numbers(1);

            if ((unitHeat.Fan_Index = Fans::GetFanIndex(state, unitHeat.FanName)) == 0) {
                ShowSevereItemNotFound(state, eoh, cAlphaFields(6), unitHeat.FanName);
                ErrorsFound = true;

            } else {
                auto *fan = state.dataFans->fans(unitHeat.Fan_Index);

                unitHeat.FanOutletNode = fan->outletNodeNum;

                FanVolFlow = fan->maxAirFlowRate;

                if (FanVolFlow != AutoSize && unitHeat.MaxAirVolFlow != AutoSize && FanVolFlow < unitHeat.MaxAirVolFlow) {
                    ShowSevereError(state, format("Specified in {} = {}", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                    ShowContinueError(
                        state,
                        format("...air flow rate ({:.7T}) in fan object {} is less than the unit heater maximum supply air flow rate ({:.7T}).",
                               FanVolFlow,
                               state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanName,
                               state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow));
                    ShowContinueError(state, "...the fan flow rate must be greater than or equal to the unit heater maximum supply air flow rate.");
                    ErrorsFound = true;
                } else if (FanVolFlow == AutoSize && state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow != AutoSize) {
                    ShowWarningError(state, format("Specified in {} = {}", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                    ShowContinueError(state, "...the fan flow rate is autosized while the unit heater flow rate is not.");
                    ShowContinueError(state, "...this can lead to unexpected results where the fan flow rate is less than required.");
                } else if (FanVolFlow != AutoSize && state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow == AutoSize) {
                    ShowWarningError(state, format("Specified in {} = {}", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                    ShowContinueError(state, "...the unit heater flow rate is autosized while the fan flow rate is not.");
                    ShowContinueError(state, "...this can lead to unexpected results where the fan flow rate is less than required.");
                }
                unitHeat.FanAvailSchedPtr = fan->availSchedNum;
            }

            // Heating coil information:
            {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type =
                    static_cast<HCoilType>(getEnumValue(HCoilTypeNamesUC, Util::makeUPPER(Alphas(7))));
                switch (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type) {
                case HCoilType::WaterHeatingCoil:
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatingCoilType = DataPlant::PlantEquipmentType::CoilWaterSimpleHeating;
                    break;
                case HCoilType::SteamCoil:
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatingCoilType = DataPlant::PlantEquipmentType::CoilSteamAirHeating;
                    break;
                case HCoilType::Electric:
                case HCoilType::Gas:
                    break;
                default: {
                    ShowSevereError(state, format("Illegal {} = {}", cAlphaFields(7), Alphas(7)));
                    ShowContinueError(state, format("Occurs in {}={}", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                    ErrorsFound = true;
                    errFlag = true;
                }
                }
            }
            if (!errFlag) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilTypeCh = Alphas(7);
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName = Alphas(8);
                ValidateComponent(state, Alphas(7), state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, IsNotOK, CurrentModuleObject);
                if (IsNotOK) {
                    ShowContinueError(state,
                                      format("specified in {} = \"{}\"", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                    ErrorsFound = true;
                } else {
                    // The heating coil control node is necessary for hot water and steam coils, but not necessary for an
                    // electric or gas coil.
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::WaterHeatingCoil ||
                        state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::SteamCoil) {
                        // mine the hot water or steam node from the coil object
                        errFlag = false;
                        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::WaterHeatingCoil) {
                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode =
                                GetCoilWaterInletNode(state, "Coil:Heating:Water", state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, errFlag);
                        } else { // its a steam coil
                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index =
                                GetSteamCoilIndex(state, "COIL:HEATING:STEAM", state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, errFlag);
                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode =
                                GetCoilSteamInletNode(state,
                                                      state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index,
                                                      state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                                      errFlag);
                        }
                        // Other error checks should trap before it gets to this point in the code, but including just in case.
                        if (errFlag) {
                            ShowContinueError(
                                state,
                                format("that was specified in {} = \"{}\"", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                            ErrorsFound = true;
                        }
                    }
                }
            }

            state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanSchedPtr = GetScheduleIndex(state, Alphas(9));
            // Default to cycling fan when fan operating mode schedule is not present
            if (!lAlphaBlanks(9) && state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanSchedPtr == 0) {
                ShowSevereError(state,
                                format("{} \"{}\" {} not found: {}",
                                       CurrentModuleObject,
                                       state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                       cAlphaFields(9),
                                       Alphas(9)));
                ErrorsFound = true;
            } else if (lAlphaBlanks(9)) {
                if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanType == HVAC::FanType::OnOff ||
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanType == HVAC::FanType::SystemModel) {
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanOp = HVAC::FanOp::Cycling;
                } else {
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanOp = HVAC::FanOp::Continuous;
                }
            }

            // Check fan's schedule for cycling fan operation if constant volume fan is used
            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanSchedPtr > 0 &&
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanType == HVAC::FanType::Constant) {
                if (!CheckScheduleValueMinMax(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanSchedPtr, ">", 0.0, "<=", 1.0)) {
                    ShowSevereError(state, format("{} = {}", CurrentModuleObject, Alphas(1)));
                    ShowContinueError(state, format("For {} = {}", cAlphaFields(5), Alphas(5)));
                    ShowContinueError(state, "Fan operating mode must be continuous (fan operating mode schedule values > 0).");
                    ShowContinueError(state, format("Error found in {} = {}", cAlphaFields(9), Alphas(9)));
                    ShowContinueError(state, "...schedule values must be (>0., <=1.)");
                    ErrorsFound = true;
                }
            }

            state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOperatesDuringNoHeating = Alphas(10);
            if ((!Util::SameString(state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOperatesDuringNoHeating, "Yes")) &&
                (!Util::SameString(state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOperatesDuringNoHeating, "No"))) {
                ErrorsFound = true;
                ShowSevereError(state, format("Illegal {} = {}", cAlphaFields(10), Alphas(10)));
                ShowContinueError(state, format("Occurs in {}={}", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
            } else if (Util::SameString(state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOperatesDuringNoHeating, "No")) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOffNoHeating = true;
            }

            state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow = Numbers(2);
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinVolHotWaterFlow = Numbers(3);
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow = Numbers(2);
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinVolHotSteamFlow = Numbers(3);

            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlOffset = Numbers(4);
            // Set default convergence tolerance
            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlOffset <= 0.0) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlOffset = 0.001;
            }

            if (!lAlphaBlanks(11)) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).AvailManagerListName = Alphas(11);
            }

            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex = 0;
            if (!lAlphaBlanks(12)) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex = Util::FindItemInList(Alphas(12), state.dataSize->ZoneHVACSizing);
                if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex == 0) {
                    ShowSevereError(state, format("{} = {} not found.", cAlphaFields(12), Alphas(12)));
                    ShowContinueError(state, format("Occurs in {} = {}", CurrentModuleObject, state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                    ErrorsFound = true;
                }
            }

            // check that unit heater air inlet node must be the same as a zone exhaust node
            ZoneNodeNotFound = true;
            for (CtrlZone = 1; CtrlZone <= state.dataGlobal->NumOfZones; ++CtrlZone) {
                if (!state.dataZoneEquip->ZoneEquipConfig(CtrlZone).IsControlled) continue;
                for (NodeNum = 1; NodeNum <= state.dataZoneEquip->ZoneEquipConfig(CtrlZone).NumExhaustNodes; ++NodeNum) {
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode ==
                        state.dataZoneEquip->ZoneEquipConfig(CtrlZone).ExhaustNode(NodeNum)) {
                        ZoneNodeNotFound = false;
                        break;
                    }
                }
            }
            if (ZoneNodeNotFound) {
                ShowSevereError(state,
                                format("{} = \"{}\". Unit heater air inlet node name must be the same as a zone exhaust node name.",
                                       CurrentModuleObject,
                                       state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                ShowContinueError(state, "..Zone exhaust node name is specified in ZoneHVAC:EquipmentConnections object.");
                ShowContinueError(state,
                                  format("..Unit heater air inlet node name = {}",
                                         state.dataLoopNodes->NodeID(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode)));
                ErrorsFound = true;
            }
            // check that unit heater air outlet node is a zone inlet node.
            ZoneNodeNotFound = true;
            for (CtrlZone = 1; CtrlZone <= state.dataGlobal->NumOfZones; ++CtrlZone) {
                if (!state.dataZoneEquip->ZoneEquipConfig(CtrlZone).IsControlled) continue;
                for (NodeNum = 1; NodeNum <= state.dataZoneEquip->ZoneEquipConfig(CtrlZone).NumInletNodes; ++NodeNum) {
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode ==
                        state.dataZoneEquip->ZoneEquipConfig(CtrlZone).InletNode(NodeNum)) {
                        state.dataUnitHeaters->UnitHeat(UnitHeatNum).ZonePtr = CtrlZone;
                        ZoneNodeNotFound = false;
                        break;
                    }
                }
            }
            if (ZoneNodeNotFound) {
                ShowSevereError(state,
                                format("{} = \"{}\". Unit heater air outlet node name must be the same as a zone inlet node name.",
                                       CurrentModuleObject,
                                       state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                ShowContinueError(state, "..Zone inlet node name is specified in ZoneHVAC:EquipmentConnections object.");
                ShowContinueError(state,
                                  format("..Unit heater air outlet node name = {}",
                                         state.dataLoopNodes->NodeID(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode)));
                ErrorsFound = true;
            }

            // Add fan to component sets array
            SetUpCompSets(state,
                          CurrentModuleObject,
                          state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                          HVAC::fanTypeNamesUC[(int)state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanType],
                          state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanName,
                          state.dataLoopNodes->NodeID(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode),
                          state.dataLoopNodes->NodeID(state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOutletNode));

            // Add heating coil to component sets array
            SetUpCompSets(state,
                          CurrentModuleObject,
                          state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                          state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilTypeCh,
                          state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                          state.dataLoopNodes->NodeID(state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOutletNode),
                          state.dataLoopNodes->NodeID(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode));

        } // ...loop over all of the unit heaters found in the input file

        Alphas.deallocate();
        Numbers.deallocate();
        cAlphaFields.deallocate();
        cNumericFields.deallocate();
        lAlphaBlanks.deallocate();
        lNumericBlanks.deallocate();

        if (ErrorsFound) ShowFatalError(state, format("{}Errors found in input", RoutineName));

        // Setup Report variables for the Unit Heaters, CurrentModuleObject='ZoneHVAC:UnitHeater'
        for (UnitHeatNum = 1; UnitHeatNum <= state.dataUnitHeaters->NumOfUnitHeats; ++UnitHeatNum) {
            SetupOutputVariable(state,
                                "Zone Unit Heater Heating Rate",
                                Constant::Units::W,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatPower,
                                OutputProcessor::TimeStepType::System,
                                OutputProcessor::StoreType::Average,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);
            SetupOutputVariable(state,
                                "Zone Unit Heater Heating Energy",
                                Constant::Units::J,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatEnergy,
                                OutputProcessor::TimeStepType::System,
                                OutputProcessor::StoreType::Sum,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);
            SetupOutputVariable(state,
                                "Zone Unit Heater Fan Electricity Rate",
                                Constant::Units::W,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).ElecPower,
                                OutputProcessor::TimeStepType::System,
                                OutputProcessor::StoreType::Average,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);
            // Note that the unit heater fan electric is NOT metered because this value is already metered through the fan component
            SetupOutputVariable(state,
                                "Zone Unit Heater Fan Electricity Energy",
                                Constant::Units::J,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).ElecEnergy,
                                OutputProcessor::TimeStepType::System,
                                OutputProcessor::StoreType::Sum,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);
            SetupOutputVariable(state,
                                "Zone Unit Heater Fan Availability Status",
                                Constant::Units::None,
                                (int &)state.dataUnitHeaters->UnitHeat(UnitHeatNum).availStatus,
                                OutputProcessor::TimeStepType::System,
                                OutputProcessor::StoreType::Average,
                                state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);
            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanType == HVAC::FanType::OnOff) {
                SetupOutputVariable(state,
                                    "Zone Unit Heater Fan Part Load Ratio",
                                    Constant::Units::None,
                                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanPartLoadRatio,
                                    OutputProcessor::TimeStepType::System,
                                    OutputProcessor::StoreType::Average,
                                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);
            }
        }

        for (UnitHeatNum = 1; UnitHeatNum <= state.dataUnitHeaters->NumOfUnitHeats; ++UnitHeatNum) {
            auto &unitHeat = state.dataUnitHeaters->UnitHeat(UnitHeatNum);
            state.dataRptCoilSelection->coilSelectionReportObj->setCoilSupplyFanInfo(
                state, unitHeat.HCoilName, unitHeat.HCoilTypeCh, unitHeat.FanName, unitHeat.fanType, unitHeat.Fan_Index);
        }
    }

    void InitUnitHeater(EnergyPlusData &state,
                        int const UnitHeatNum,                         // index for the current unit heater
                        int const ZoneNum,                             // number of zone being served
                        [[maybe_unused]] bool const FirstHVACIteration // TRUE if 1st HVAC simulation of system timestep
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   May 2000
        //       MODIFIED       Chandan Sharma, FSEC, March 2011: Added ZoneHVAC sys avail manager
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine initializes all of the data elements which are necessary
        // to simulate a unit heater.

        // METHODOLOGY EMPLOYED:
        // Uses the status flags to trigger initializations.

        // Using/Aliasing
        using DataZoneEquipment::CheckZoneEquipmentList;
        using FluidProperties::GetDensityGlycol;
        using PlantUtilities::InitComponentNodes;
        using PlantUtilities::ScanPlantLoopsForObject;
        using PlantUtilities::SetComponentFlowRate;
        using namespace DataZoneEnergyDemands;
        using WaterCoils::SimulateWaterCoilComponents;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("InitUnitHeater");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int Loop;
        int HotConNode; // hot water control node number in unit heater loop
        int InNode;     // inlet node number in unit heater loop
        int OutNode;    // outlet node number in unit heater loop
        Real64 RhoAir;  // air density at InNode
        Real64 TempSteamIn;
        Real64 SteamDensity;
        Real64 rho; // local fluid density
        bool errFlag;

        // Do the one time initializations
        if (state.dataUnitHeaters->InitUnitHeaterOneTimeFlag) {

            state.dataUnitHeaters->MyEnvrnFlag.allocate(state.dataUnitHeaters->NumOfUnitHeats);
            state.dataUnitHeaters->MySizeFlag.allocate(state.dataUnitHeaters->NumOfUnitHeats);
            state.dataUnitHeaters->MyPlantScanFlag.allocate(state.dataUnitHeaters->NumOfUnitHeats);
            state.dataUnitHeaters->MyZoneEqFlag.allocate(state.dataUnitHeaters->NumOfUnitHeats);
            state.dataUnitHeaters->MyEnvrnFlag = true;
            state.dataUnitHeaters->MySizeFlag = true;
            state.dataUnitHeaters->MyPlantScanFlag = true;
            state.dataUnitHeaters->MyZoneEqFlag = true;
            state.dataUnitHeaters->InitUnitHeaterOneTimeFlag = false;
        }

        if (allocated(state.dataAvail->ZoneComp)) {
            auto &availMgr = state.dataAvail->ZoneComp(DataZoneEquipment::ZoneEquipType::UnitHeater).ZoneCompAvailMgrs(UnitHeatNum);
            if (state.dataUnitHeaters->MyZoneEqFlag(UnitHeatNum)) { // initialize the name of each availability manager list and zone number
                availMgr.AvailManagerListName = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AvailManagerListName;
                availMgr.ZoneNum = ZoneNum;
                state.dataUnitHeaters->MyZoneEqFlag(UnitHeatNum) = false;
            }
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).availStatus = availMgr.availStatus;
        }

        if (state.dataUnitHeaters->MyPlantScanFlag(UnitHeatNum) && allocated(state.dataPlnt->PlantLoop)) {
            if ((state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatingCoilType == DataPlant::PlantEquipmentType::CoilWaterSimpleHeating) ||
                (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatingCoilType == DataPlant::PlantEquipmentType::CoilSteamAirHeating)) {
                errFlag = false;
                ScanPlantLoopsForObject(state,
                                        state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                        state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatingCoilType,
                                        state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc,
                                        errFlag,
                                        _,
                                        _,
                                        _,
                                        _,
                                        _);
                if (errFlag) {
                    ShowContinueError(state,
                                      format("Reference Unit=\"{}\", type=ZoneHVAC:UnitHeater", state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                    ShowFatalError(state, "InitUnitHeater: Program terminated due to previous condition(s).");
                }

                state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum =
                    DataPlant::CompData::getPlantComponent(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc).NodeNumOut;
            }
            state.dataUnitHeaters->MyPlantScanFlag(UnitHeatNum) = false;
        } else if (state.dataUnitHeaters->MyPlantScanFlag(UnitHeatNum) && !state.dataGlobal->AnyPlantInModel) {
            state.dataUnitHeaters->MyPlantScanFlag(UnitHeatNum) = false;
        }
        // need to check all units to see if they are on Zone Equipment List or issue warning
        if (!state.dataUnitHeaters->ZoneEquipmentListChecked && state.dataZoneEquip->ZoneEquipInputsFilled) {
            state.dataUnitHeaters->ZoneEquipmentListChecked = true;
            for (Loop = 1; Loop <= state.dataUnitHeaters->NumOfUnitHeats; ++Loop) {
                if (CheckZoneEquipmentList(state, "ZoneHVAC:UnitHeater", state.dataUnitHeaters->UnitHeat(Loop).Name)) continue;
                ShowSevereError(state,
                                format("InitUnitHeater: Unit=[UNIT HEATER,{}] is not on any ZoneHVAC:EquipmentList.  It will not be simulated.",
                                       state.dataUnitHeaters->UnitHeat(Loop).Name));
            }
        }

        if (!state.dataGlobal->SysSizingCalc && state.dataUnitHeaters->MySizeFlag(UnitHeatNum) &&
            !state.dataUnitHeaters->MyPlantScanFlag(UnitHeatNum)) {

            SizeUnitHeater(state, UnitHeatNum);

            state.dataUnitHeaters->MySizeFlag(UnitHeatNum) = false;
        } // Do the one time initializations

        if (state.dataGlobal->BeginEnvrnFlag && state.dataUnitHeaters->MyEnvrnFlag(UnitHeatNum) &&
            !state.dataUnitHeaters->MyPlantScanFlag(UnitHeatNum)) {
            InNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode;
            OutNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode;
            HotConNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode;
            RhoAir = state.dataEnvrn->StdRhoAir;

            // set the mass flow rates from the input volume flow rates
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow = RhoAir * state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow;

            // set the node max and min mass flow rates
            state.dataLoopNodes->Node(OutNode).MassFlowRateMax = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
            state.dataLoopNodes->Node(OutNode).MassFlowRateMin = 0.0;

            state.dataLoopNodes->Node(InNode).MassFlowRateMax = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
            state.dataLoopNodes->Node(InNode).MassFlowRateMin = 0.0;

            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::WaterHeatingCoil) {
                rho = GetDensityGlycol(state,
                                       state.dataPlnt->PlantLoop(state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum).FluidName,
                                       Constant::HWInitConvTemp,
                                       state.dataPlnt->PlantLoop(state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum).FluidIndex,
                                       RoutineName);

                state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxHotWaterFlow = rho * state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow;
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinHotWaterFlow = rho * state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinVolHotWaterFlow;
                InitComponentNodes(state,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinHotWaterFlow,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxHotWaterFlow,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum);
            }
            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::SteamCoil) {
                TempSteamIn = 100.00;
                SteamDensity = GetSatDensityRefrig(
                    state, fluidNameSteam, TempSteamIn, 1.0, state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_FluidIndex, RoutineName);
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxHotSteamFlow =
                    SteamDensity * state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow;
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinHotSteamFlow =
                    SteamDensity * state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinVolHotSteamFlow;

                InitComponentNodes(state,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinHotSteamFlow,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxHotSteamFlow,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum);
            }

            state.dataUnitHeaters->MyEnvrnFlag(UnitHeatNum) = false;
        } // ...end start of environment inits

        if (!state.dataGlobal->BeginEnvrnFlag) state.dataUnitHeaters->MyEnvrnFlag(UnitHeatNum) = true;

        // These initializations are done every iteration...
        InNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode;
        OutNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode;

        state.dataUnitHeaters->QZnReq = state.dataZoneEnergyDemand->ZoneSysEnergyDemand(ZoneNum).RemainingOutputReqToHeatSP; // zone load needed
        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanSchedPtr > 0) {
            if (GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanSchedPtr) == 0.0 &&
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanType == HVAC::FanType::OnOff) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanOp = HVAC::FanOp::Cycling;
            } else {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanOp = HVAC::FanOp::Continuous;
            }
            if ((state.dataUnitHeaters->QZnReq < SmallLoad) || state.dataZoneEnergyDemand->CurDeadBandOrSetback(ZoneNum)) {
                // Unit is available, but there is no load on it or we are in setback/deadband
                if (!state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOffNoHeating &&
                    GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanSchedPtr) > 0.0) {
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanOp = HVAC::FanOp::Continuous;
                }
            }
        }

        state.dataUnitHeaters->SetMassFlowRateToZero = false;
        if (GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).SchedPtr) > 0) {
            if ((GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanAvailSchedPtr) > 0 ||
                 state.dataHVACGlobal->TurnFansOn) &&
                !state.dataHVACGlobal->TurnFansOff) {
                if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOffNoHeating &&
                    ((state.dataZoneEnergyDemand->ZoneSysEnergyDemand(ZoneNum).RemainingOutputReqToHeatSP < SmallLoad) ||
                     (state.dataZoneEnergyDemand->CurDeadBandOrSetback(ZoneNum)))) {
                    state.dataUnitHeaters->SetMassFlowRateToZero = true;
                }
            } else {
                state.dataUnitHeaters->SetMassFlowRateToZero = true;
            }
        } else {
            state.dataUnitHeaters->SetMassFlowRateToZero = true;
        }

        if (state.dataUnitHeaters->SetMassFlowRateToZero) {
            state.dataLoopNodes->Node(InNode).MassFlowRate = 0.0;
            state.dataLoopNodes->Node(InNode).MassFlowRateMaxAvail = 0.0;
            state.dataLoopNodes->Node(InNode).MassFlowRateMinAvail = 0.0;
            state.dataLoopNodes->Node(OutNode).MassFlowRate = 0.0;
            state.dataLoopNodes->Node(OutNode).MassFlowRateMaxAvail = 0.0;
            state.dataLoopNodes->Node(OutNode).MassFlowRateMinAvail = 0.0;
        } else {
            state.dataLoopNodes->Node(InNode).MassFlowRate = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
            state.dataLoopNodes->Node(InNode).MassFlowRateMaxAvail = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
            state.dataLoopNodes->Node(InNode).MassFlowRateMinAvail = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
            state.dataLoopNodes->Node(OutNode).MassFlowRate = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
            state.dataLoopNodes->Node(OutNode).MassFlowRateMaxAvail = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
            state.dataLoopNodes->Node(OutNode).MassFlowRateMinAvail = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirMassFlow;
        }

        // Just in case the unit is off and conditions do not get sent through
        // the unit for some reason, set the outlet conditions equal to the inlet
        // conditions of the unit heater
        state.dataLoopNodes->Node(OutNode).Temp = state.dataLoopNodes->Node(InNode).Temp;
        state.dataLoopNodes->Node(OutNode).Press = state.dataLoopNodes->Node(InNode).Press;
        state.dataLoopNodes->Node(OutNode).HumRat = state.dataLoopNodes->Node(InNode).HumRat;
        state.dataLoopNodes->Node(OutNode).Enthalpy = state.dataLoopNodes->Node(InNode).Enthalpy;
    }

    void SizeUnitHeater(EnergyPlusData &state, int const UnitHeatNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   February 2002
        //       MODIFIED       August 2013 Daeho Kang, add component sizing table entries
        //                      July 2014, B. Nigusse, added scalable sizing
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for sizing Unit Heater components for which flow rates have not been
        // specified in the input.

        // METHODOLOGY EMPLOYED:
        // Obtains flow rates from the zone sizing arrays and plant sizing data.

        // Using/Aliasing
        using namespace DataSizing;
        using HVAC::HeatingAirflowSizing;
        using HVAC::HeatingCapacitySizing;
        using PlantUtilities::MyPlantSizingIndex;
        using Psychrometrics::CPHW;
        using SteamCoils::GetCoilSteamInletNode;
        using SteamCoils::GetCoilSteamOutletNode;

        // SUBROUTINE PARAMETER DEFINITIONS:
        static constexpr std::string_view RoutineName("SizeUnitHeater");

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int PltSizHeatNum; // index of plant sizing object for 1st heating loop
        bool ErrorsFound;
        Real64 DesCoilLoad;
        Real64 TempSteamIn;
        Real64 EnthSteamInDry;
        Real64 EnthSteamOutWet;
        Real64 LatentHeatSteam;
        Real64 SteamDensity;
        int CoilWaterInletNode(0);
        int CoilWaterOutletNode(0);
        int CoilSteamInletNode(0);
        int CoilSteamOutletNode(0);
        Real64 Cp;                     // local temporary for fluid specific heat
        Real64 rho;                    // local temporary for fluid density
        bool IsAutoSize;               // Indicator to autosize
        Real64 MaxAirVolFlowDes;       // Autosized maximum air flow for reporting
        Real64 MaxAirVolFlowUser;      // Hardsized maximum air flow for reporting
        Real64 MaxVolHotWaterFlowDes;  // Autosized maximum hot water flow for reporting
        Real64 MaxVolHotWaterFlowUser; // Hardsized maximum hot water flow for reporting
        Real64 MaxVolHotSteamFlowDes;  // Autosized maximum hot steam flow for reporting
        Real64 MaxVolHotSteamFlowUser; // Hardsized maximum hot steam flow for reporting
        std::string CompName;          // component name
        std::string CompType;          // component type
        std::string SizingString;      // input field sizing description (e.g., Nominal Capacity)
        Real64 TempSize;               // autosized value of coil input field
        int FieldNum = 1;              // IDD numeric field number where input field description is found
        int SizingMethod;  // Integer representation of sizing method name (e.g., CoolingAirflowSizing, HeatingAirflowSizing, CoolingCapacitySizing,
                           // HeatingCapacitySizing, etc.)
        bool PrintFlag;    // TRUE when sizing information is reported in the eio file
        int zoneHVACIndex; // index of zoneHVAC equipment sizing specification
        int SAFMethod(0);  // supply air flow rate sizing method (SupplyAirFlowRate, FlowPerFloorArea, FractionOfAutosizedCoolingAirflow,
                           // FractionOfAutosizedHeatingAirflow ...)
        int CapSizingMethod(0); // capacity sizing methods (HeatingDesignCapacity, CapacityPerFloorArea, FractionOfAutosizedCoolingCapacity, and
                                // FractionOfAutosizedHeatingCapacity )
        bool DoWaterCoilSizing = false; // if TRUE do water coil sizing calculation
        Real64 WaterCoilSizDeltaT;      // water coil deltaT for design water flow rate autosizing
        int CoilNum;                    // index of water coil object

        auto &ZoneEqSizing = state.dataSize->ZoneEqSizing;
        auto &CurZoneEqNum = state.dataSize->CurZoneEqNum;

        PltSizHeatNum = 0;
        ErrorsFound = false;
        IsAutoSize = false;
        MaxAirVolFlowDes = 0.0;
        MaxAirVolFlowUser = 0.0;
        MaxVolHotWaterFlowDes = 0.0;
        MaxVolHotWaterFlowUser = 0.0;
        MaxVolHotSteamFlowDes = 0.0;
        MaxVolHotSteamFlowUser = 0.0;

        state.dataSize->DataScalableSizingON = false;
        state.dataSize->DataScalableCapSizingON = false;
        state.dataSize->ZoneHeatingOnlyFan = true;
        CompType = "ZoneHVAC:UnitHeater";
        CompName = state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name;
        state.dataSize->DataZoneNumber = state.dataUnitHeaters->UnitHeat(UnitHeatNum).ZonePtr;
        state.dataSize->DataFanType = state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanType;
        state.dataSize->DataFanIndex = state.dataUnitHeaters->UnitHeat(UnitHeatNum).Fan_Index;
        // unit heater is always blow thru
        state.dataSize->DataFanPlacement = HVAC::FanPlace::BlowThru;

        if (CurZoneEqNum > 0) {
            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex > 0) {
                zoneHVACIndex = state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex;
                SizingMethod = HeatingAirflowSizing;
                FieldNum = 1; //  N1 , \field Maximum Supply Air Flow Rate
                PrintFlag = true;
                SizingString = state.dataUnitHeaters->UnitHeatNumericFields(UnitHeatNum).FieldNames(FieldNum) + " [m3/s]";
                SAFMethod = state.dataSize->ZoneHVACSizing(zoneHVACIndex).HeatingSAFMethod;
                ZoneEqSizing(CurZoneEqNum).SizingMethod(SizingMethod) = SAFMethod;
                if (SAFMethod == None || SAFMethod == SupplyAirFlowRate || SAFMethod == FlowPerFloorArea ||
                    SAFMethod == FractionOfAutosizedHeatingAirflow) {
                    if (SAFMethod == SupplyAirFlowRate) {
                        if (state.dataSize->ZoneHVACSizing(zoneHVACIndex).MaxHeatAirVolFlow > 0.0) {
                            ZoneEqSizing(CurZoneEqNum).AirVolFlow = state.dataSize->ZoneHVACSizing(zoneHVACIndex).MaxHeatAirVolFlow;
                            ZoneEqSizing(CurZoneEqNum).SystemAirFlow = true;
                        }
                        TempSize = state.dataSize->ZoneHVACSizing(zoneHVACIndex).MaxHeatAirVolFlow;
                    } else if (SAFMethod == FlowPerFloorArea) {
                        ZoneEqSizing(CurZoneEqNum).SystemAirFlow = true;
                        ZoneEqSizing(CurZoneEqNum).AirVolFlow = state.dataSize->ZoneHVACSizing(zoneHVACIndex).MaxHeatAirVolFlow *
                                                                state.dataHeatBal->Zone(state.dataSize->DataZoneNumber).FloorArea;
                        TempSize = ZoneEqSizing(CurZoneEqNum).AirVolFlow;
                        state.dataSize->DataScalableSizingON = true;
                    } else if (SAFMethod == FractionOfAutosizedHeatingAirflow) {
                        state.dataSize->DataFracOfAutosizedCoolingAirflow = state.dataSize->ZoneHVACSizing(zoneHVACIndex).MaxHeatAirVolFlow;
                        TempSize = AutoSize;
                        state.dataSize->DataScalableSizingON = true;
                    } else {
                        TempSize = state.dataSize->ZoneHVACSizing(zoneHVACIndex).MaxHeatAirVolFlow;
                    }
                    bool errorsFound = false;
                    HeatingAirFlowSizer sizingHeatingAirFlow;
                    sizingHeatingAirFlow.overrideSizingString(SizingString);
                    // sizingHeatingAirFlow.setHVACSizingIndexData(FanCoil(FanCoilNum).HVACSizingIndex);
                    sizingHeatingAirFlow.initializeWithinEP(state, CompType, CompName, PrintFlag, RoutineName);
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow = sizingHeatingAirFlow.size(state, TempSize, errorsFound);

                } else if (SAFMethod == FlowPerHeatingCapacity) {
                    SizingMethod = HeatingCapacitySizing;
                    TempSize = AutoSize;
                    PrintFlag = false;
                    state.dataSize->DataScalableSizingON = true;
                    state.dataSize->DataFlowUsedForSizing = state.dataSize->FinalZoneSizing(CurZoneEqNum).DesHeatVolFlow;
                    bool errorsFound = false;
                    HeatingCapacitySizer sizerHeatingCapacity;
                    sizerHeatingCapacity.overrideSizingString(SizingString);
                    sizerHeatingCapacity.initializeWithinEP(state, CompType, CompName, PrintFlag, RoutineName);
                    TempSize = sizerHeatingCapacity.size(state, TempSize, errorsFound);
                    if (state.dataSize->ZoneHVACSizing(zoneHVACIndex).HeatingCapMethod == FractionOfAutosizedHeatingCapacity) {
                        state.dataSize->DataFracOfAutosizedHeatingCapacity = state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity;
                    }
                    state.dataSize->DataAutosizedHeatingCapacity = TempSize;
                    state.dataSize->DataFlowPerHeatingCapacity = state.dataSize->ZoneHVACSizing(zoneHVACIndex).MaxHeatAirVolFlow;
                    SizingMethod = HeatingAirflowSizing;
                    PrintFlag = true;
                    TempSize = AutoSize;
                    errorsFound = false;
                    HeatingAirFlowSizer sizingHeatingAirFlow;
                    sizingHeatingAirFlow.overrideSizingString(SizingString);
                    // sizingHeatingAirFlow.setHVACSizingIndexData(FanCoil(FanCoilNum).HVACSizingIndex);
                    sizingHeatingAirFlow.initializeWithinEP(state, CompType, CompName, PrintFlag, RoutineName);
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow = sizingHeatingAirFlow.size(state, TempSize, errorsFound);
                }
                state.dataSize->DataScalableSizingON = false;
            } else {
                // no scalble sizing method has been specified. Sizing proceeds using the method
                // specified in the zoneHVAC object
                SizingMethod = HeatingAirflowSizing;
                FieldNum = 1; // N1 , \field Maximum Supply Air Flow Rate
                PrintFlag = true;
                SizingString = state.dataUnitHeaters->UnitHeatNumericFields(UnitHeatNum).FieldNames(FieldNum) + " [m3/s]";
                TempSize = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow;
                bool errorsFound = false;
                HeatingAirFlowSizer sizingHeatingAirFlow;
                sizingHeatingAirFlow.overrideSizingString(SizingString);
                // sizingHeatingAirFlow.setHVACSizingIndexData(FanCoil(FanCoilNum).HVACSizingIndex);
                sizingHeatingAirFlow.initializeWithinEP(state, CompType, CompName, PrintFlag, RoutineName);
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow = sizingHeatingAirFlow.size(state, TempSize, errorsFound);
            }
        }

        IsAutoSize = false;
        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow == AutoSize) {
            IsAutoSize = true;
        }

        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::WaterHeatingCoil) {

            if (CurZoneEqNum > 0) {
                if (!IsAutoSize && !state.dataSize->ZoneSizingRunDone) { // Simulation continue
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow > 0.0) {
                        BaseSizer::reportSizerOutput(state,
                                                     "ZoneHVAC:UnitHeater",
                                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                                     "User-Specified Maximum Hot Water Flow [m3/s]",
                                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow);
                    }
                } else {
                    CheckZoneSizing(state, "ZoneHVAC:UnitHeater", state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);

                    CoilWaterInletNode = WaterCoils::GetCoilWaterInletNode(
                        state, "Coil:Heating:Water", state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, ErrorsFound);
                    CoilWaterOutletNode = WaterCoils::GetCoilWaterOutletNode(
                        state, "Coil:Heating:Water", state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, ErrorsFound);
                    if (IsAutoSize) {
                        PltSizHeatNum = MyPlantSizingIndex(state,
                                                           "Coil:Heating:Water",
                                                           state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                                           CoilWaterInletNode,
                                                           CoilWaterOutletNode,
                                                           ErrorsFound);
                        CoilNum = WaterCoils::GetWaterCoilIndex(
                            state, "COIL:HEATING:WATER", state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, ErrorsFound);
                        if (state.dataWaterCoils->WaterCoil(CoilNum).UseDesignWaterDeltaTemp) {
                            WaterCoilSizDeltaT = state.dataWaterCoils->WaterCoil(CoilNum).DesignWaterDeltaTemp;
                            DoWaterCoilSizing = true;
                        } else {
                            if (PltSizHeatNum > 0) {
                                WaterCoilSizDeltaT = state.dataSize->PlantSizData(PltSizHeatNum).DeltaT;
                                DoWaterCoilSizing = true;
                            } else {
                                DoWaterCoilSizing = false;
                                // If there is no heating Plant Sizing object and autosizing was requested, issue fatal error message
                                ShowSevereError(state, "Autosizing of water coil requires a heating loop Sizing:Plant object");
                                ShowContinueError(
                                    state, format("Occurs in ZoneHVAC:UnitHeater Object={}", state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                                ErrorsFound = true;
                            }
                        }

                        if (DoWaterCoilSizing) {
                            SizingMethod = HeatingCapacitySizing;
                            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex > 0) {
                                zoneHVACIndex = state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex;
                                CapSizingMethod = state.dataSize->ZoneHVACSizing(zoneHVACIndex).HeatingCapMethod;
                                ZoneEqSizing(CurZoneEqNum).SizingMethod(SizingMethod) = CapSizingMethod;
                                if (CapSizingMethod == HeatingDesignCapacity || CapSizingMethod == CapacityPerFloorArea ||
                                    CapSizingMethod == FractionOfAutosizedHeatingCapacity) {
                                    if (CapSizingMethod == HeatingDesignCapacity) {
                                        if (state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity == AutoSize) {
                                            ZoneEqSizing(CurZoneEqNum).DesHeatingLoad = state.dataSize->FinalZoneSizing(CurZoneEqNum).DesHeatLoad;
                                        } else {
                                            ZoneEqSizing(CurZoneEqNum).DesHeatingLoad =
                                                state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity;
                                        }
                                        ZoneEqSizing(CurZoneEqNum).HeatingCapacity = true;
                                        TempSize = AutoSize;
                                    } else if (CapSizingMethod == CapacityPerFloorArea) {
                                        ZoneEqSizing(CurZoneEqNum).HeatingCapacity = true;
                                        ZoneEqSizing(CurZoneEqNum).DesHeatingLoad =
                                            state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity *
                                            state.dataHeatBal->Zone(state.dataSize->DataZoneNumber).FloorArea;
                                        state.dataSize->DataScalableCapSizingON = true;
                                    } else if (CapSizingMethod == FractionOfAutosizedHeatingCapacity) {
                                        state.dataSize->DataFracOfAutosizedHeatingCapacity =
                                            state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity;
                                        state.dataSize->DataScalableCapSizingON = true;
                                        TempSize = AutoSize;
                                    }
                                }
                                PrintFlag = false;
                                bool errorsFound = false;
                                HeatingCapacitySizer sizerHeatingCapacity;
                                sizerHeatingCapacity.overrideSizingString(SizingString);
                                sizerHeatingCapacity.initializeWithinEP(state, CompType, CompName, PrintFlag, RoutineName);
                                DesCoilLoad = sizerHeatingCapacity.size(state, TempSize, errorsFound);
                                state.dataSize->DataScalableCapSizingON = false;
                            } else {
                                SizingString = "";
                                PrintFlag = false;
                                TempSize = AutoSize;
                                ZoneEqSizing(CurZoneEqNum).HeatingCapacity = true;
                                ZoneEqSizing(CurZoneEqNum).DesHeatingLoad = state.dataSize->FinalZoneSizing(CurZoneEqNum).DesHeatLoad;
                                bool errorsFound = false;
                                HeatingCapacitySizer sizerHeatingCapacity;
                                sizerHeatingCapacity.overrideSizingString(SizingString);
                                sizerHeatingCapacity.initializeWithinEP(state, CompType, CompName, PrintFlag, RoutineName);
                                DesCoilLoad = sizerHeatingCapacity.size(state, TempSize, errorsFound);
                            }

                            if (DesCoilLoad >= SmallLoad) {
                                rho = GetDensityGlycol(
                                    state,
                                    state.dataPlnt->PlantLoop(state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum).FluidName,
                                    Constant::HWInitConvTemp,
                                    state.dataPlnt->PlantLoop(state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum).FluidIndex,
                                    RoutineName);
                                Cp = GetSpecificHeatGlycol(
                                    state,
                                    state.dataPlnt->PlantLoop(state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum).FluidName,
                                    Constant::HWInitConvTemp,
                                    state.dataPlnt->PlantLoop(state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum).FluidIndex,
                                    RoutineName);
                                MaxVolHotWaterFlowDes = DesCoilLoad / (WaterCoilSizDeltaT * Cp * rho);
                            } else {
                                MaxVolHotWaterFlowDes = 0.0;
                            }
                        }
                    }
                    if (IsAutoSize) {
                        state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow = MaxVolHotWaterFlowDes;
                        BaseSizer::reportSizerOutput(state,
                                                     "ZoneHVAC:UnitHeater",
                                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                                     "Design Size Maximum Hot Water Flow [m3/s]",
                                                     MaxVolHotWaterFlowDes);
                    } else {
                        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow > 0.0 && MaxVolHotWaterFlowDes > 0.0) {
                            MaxVolHotWaterFlowUser = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow;
                            BaseSizer::reportSizerOutput(state,
                                                         "ZoneHVAC:UnitHeater",
                                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                                         "Design Size Maximum Hot Water Flow [m3/s]",
                                                         MaxVolHotWaterFlowDes,
                                                         "User-Specified Maximum Hot Water Flow [m3/s]",
                                                         MaxVolHotWaterFlowUser);
                            if (state.dataGlobal->DisplayExtraWarnings) {
                                if ((std::abs(MaxVolHotWaterFlowDes - MaxVolHotWaterFlowUser) / MaxVolHotWaterFlowUser) >
                                    state.dataSize->AutoVsHardSizingThreshold) {
                                    ShowMessage(state,
                                                format("SizeUnitHeater: Potential issue with equipment sizing for ZoneHVAC:UnitHeater {}",
                                                       state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                                    ShowContinueError(state,
                                                      format("User-Specified Maximum Hot Water Flow of {:.5R} [m3/s]", MaxVolHotWaterFlowUser));
                                    ShowContinueError(
                                        state, format("differs from Design Size Maximum Hot Water Flow of {:.5R} [m3/s]", MaxVolHotWaterFlowDes));
                                    ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                    }
                }
            }
        } else {
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow = 0.0;
        }

        IsAutoSize = false;
        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow == AutoSize) {
            IsAutoSize = true;
        }

        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::SteamCoil) {

            if (CurZoneEqNum > 0) {
                if (!IsAutoSize && !state.dataSize->ZoneSizingRunDone) { // Simulation continue
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow > 0.0) {
                        BaseSizer::reportSizerOutput(state,
                                                     "ZoneHVAC:UnitHeater",
                                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                                     "User-Specified Maximum Steam Flow [m3/s]",
                                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow);
                    }
                } else {
                    CheckZoneSizing(state, "ZoneHVAC:UnitHeater", state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name);

                    CoilSteamInletNode =
                        GetCoilSteamInletNode(state, "Coil:Heating:Steam", state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, ErrorsFound);
                    CoilSteamOutletNode =
                        GetCoilSteamInletNode(state, "Coil:Heating:Steam", state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName, ErrorsFound);
                    if (IsAutoSize) {
                        PltSizHeatNum = MyPlantSizingIndex(state,
                                                           "Coil:Heating:Steam",
                                                           state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                                           CoilSteamInletNode,
                                                           CoilSteamOutletNode,
                                                           ErrorsFound);
                        if (PltSizHeatNum > 0) {
                            if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex > 0) {
                                zoneHVACIndex = state.dataUnitHeaters->UnitHeat(UnitHeatNum).HVACSizingIndex;
                                SizingMethod = HeatingCapacitySizing;
                                CapSizingMethod = state.dataSize->ZoneHVACSizing(zoneHVACIndex).HeatingCapMethod;
                                ZoneEqSizing(CurZoneEqNum).SizingMethod(SizingMethod) = CapSizingMethod;
                                if (CapSizingMethod == HeatingDesignCapacity || CapSizingMethod == CapacityPerFloorArea ||
                                    CapSizingMethod == FractionOfAutosizedHeatingCapacity) {
                                    if (CapSizingMethod == HeatingDesignCapacity) {
                                        if (state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity == AutoSize) {
                                            ZoneEqSizing(CurZoneEqNum).DesHeatingLoad = state.dataSize->FinalZoneSizing(CurZoneEqNum).DesHeatLoad;
                                        } else {
                                            ZoneEqSizing(CurZoneEqNum).DesHeatingLoad =
                                                state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity;
                                        }
                                        ZoneEqSizing(CurZoneEqNum).HeatingCapacity = true;
                                        TempSize = AutoSize;
                                    } else if (CapSizingMethod == CapacityPerFloorArea) {
                                        ZoneEqSizing(CurZoneEqNum).HeatingCapacity = true;
                                        ZoneEqSizing(CurZoneEqNum).DesHeatingLoad =
                                            state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity *
                                            state.dataHeatBal->Zone(state.dataSize->DataZoneNumber).FloorArea;
                                        state.dataSize->DataScalableCapSizingON = true;
                                    } else if (CapSizingMethod == FractionOfAutosizedHeatingCapacity) {
                                        state.dataSize->DataFracOfAutosizedHeatingCapacity =
                                            state.dataSize->ZoneHVACSizing(zoneHVACIndex).ScaledHeatingCapacity;
                                        TempSize = AutoSize;
                                        state.dataSize->DataScalableCapSizingON = true;
                                    }
                                }
                                SizingMethod = HeatingCapacitySizing;
                                PrintFlag = false;
                                bool errorsFound = false;
                                HeatingCapacitySizer sizerHeatingCapacity;
                                sizerHeatingCapacity.overrideSizingString(SizingString);
                                sizerHeatingCapacity.initializeWithinEP(state, CompType, CompName, PrintFlag, RoutineName);
                                DesCoilLoad = sizerHeatingCapacity.size(state, TempSize, errorsFound);
                                state.dataSize->DataScalableCapSizingON = false;
                            } else {
                                DesCoilLoad = state.dataSize->FinalZoneSizing(CurZoneEqNum).DesHeatLoad;
                            }
                            if (DesCoilLoad >= SmallLoad) {
                                TempSteamIn = 100.00;
                                EnthSteamInDry =
                                    GetSatEnthalpyRefrig(state, fluidNameSteam, TempSteamIn, 1.0, state.dataUnitHeaters->RefrigIndex, RoutineName);
                                EnthSteamOutWet =
                                    GetSatEnthalpyRefrig(state, fluidNameSteam, TempSteamIn, 0.0, state.dataUnitHeaters->RefrigIndex, RoutineName);
                                LatentHeatSteam = EnthSteamInDry - EnthSteamOutWet;
                                SteamDensity =
                                    GetSatDensityRefrig(state, fluidNameSteam, TempSteamIn, 1.0, state.dataUnitHeaters->RefrigIndex, RoutineName);
                                MaxVolHotSteamFlowDes =
                                    DesCoilLoad / (SteamDensity * (LatentHeatSteam + state.dataSize->PlantSizData(PltSizHeatNum).DeltaT *
                                                                                         CPHW(state.dataSize->PlantSizData(PltSizHeatNum).ExitTemp)));
                            } else {
                                MaxVolHotSteamFlowDes = 0.0;
                            }
                        } else {
                            ShowSevereError(state, "Autosizing of Steam flow requires a heating loop Sizing:Plant object");
                            ShowContinueError(state,
                                              format("Occurs in ZoneHVAC:UnitHeater Object={}", state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                            ErrorsFound = true;
                        }
                    }
                    if (IsAutoSize) {
                        state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow = MaxVolHotSteamFlowDes;
                        BaseSizer::reportSizerOutput(state,
                                                     "ZoneHVAC:UnitHeater",
                                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                                     "Design Size Maximum Steam Flow [m3/s]",
                                                     MaxVolHotSteamFlowDes);
                    } else {
                        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow > 0.0 && MaxVolHotSteamFlowDes > 0.0) {
                            MaxVolHotSteamFlowUser = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow;
                            BaseSizer::reportSizerOutput(state,
                                                         "ZoneHVAC:UnitHeater",
                                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                                         "Design Size Maximum Steam Flow [m3/s]",
                                                         MaxVolHotSteamFlowDes,
                                                         "User-Specified Maximum Steam Flow [m3/s]",
                                                         MaxVolHotSteamFlowUser);
                            if (state.dataGlobal->DisplayExtraWarnings) {
                                if ((std::abs(MaxVolHotSteamFlowDes - MaxVolHotSteamFlowUser) / MaxVolHotSteamFlowUser) >
                                    state.dataSize->AutoVsHardSizingThreshold) {
                                    ShowMessage(state,
                                                format("SizeUnitHeater: Potential issue with equipment sizing for ZoneHVAC:UnitHeater {}",
                                                       state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name));
                                    ShowContinueError(state, format("User-Specified Maximum Steam Flow of {:.5R} [m3/s]", MaxVolHotSteamFlowUser));
                                    ShowContinueError(state,
                                                      format("differs from Design Size Maximum Steam Flow of {:.5R} [m3/s]", MaxVolHotSteamFlowDes));
                                    ShowContinueError(state, "This may, or may not, indicate mismatched component sizes.");
                                    ShowContinueError(state, "Verify that the value entered is intended and is consistent with other components.");
                                }
                            }
                        }
                    }
                }
            }
        } else {
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotSteamFlow = 0.0;
        }

        // set the design air flow rate for the heating coil

        WaterCoils::SetCoilDesFlow(state,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilTypeCh,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                   state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxAirVolFlow,
                                   ErrorsFound);
        if (CurZoneEqNum > 0) {
            ZoneEqSizing(CurZoneEqNum).MaxHWVolFlow = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxVolHotWaterFlow;
        }

        if (ErrorsFound) {
            ShowFatalError(state, "Preceding sizing errors cause program termination");
        }
    }

    void CalcUnitHeater(EnergyPlusData &state,
                        int &UnitHeatNum,              // number of the current fan coil unit being simulated
                        int const ZoneNum,             // number of zone being served
                        bool const FirstHVACIteration, // TRUE if 1st HVAC simulation of system timestep
                        Real64 &PowerMet,              // Sensible power supplied (W)
                        Real64 &LatOutputProvided      // Latent power supplied (kg/s), negative = dehumidification
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   May 2000
        //       MODIFIED       Don Shirey, Aug 2009 (LatOutputProvided)
        //                      July 2012, Chandan Sharma - FSEC: Added zone sys avail managers
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine mainly controls the action of the unit heater
        // based on the user input for controls and the defined controls
        // algorithms.  There are currently (at the initial creation of this
        // subroutine) two control methods: on-off fan operation or continuous
        // fan operation.

        // METHODOLOGY EMPLOYED:
        // Unit is controlled based on user input and what is happening in the
        // simulation.  There are various cases to consider:
        // 1. OFF: Unit is schedule off.  All flow rates are set to zero and
        //    the temperatures are set to zone conditions.
        // 2. NO LOAD OR COOLING/ON-OFF FAN CONTROL: Unit is available, but
        //    there is no heating load.  All flow rates are set to zero and
        //    the temperatures are set to zone conditions.
        // 3. NO LOAD OR COOLING/CONTINUOUS FAN CONTROL: Unit is available and
        //    the fan is running (if it is scheduled to be available also).
        //    No heating is provided, only circulation via the fan running.
        // 4. HEATING: The unit is on/available and there is a heating load.
        //    The heating coil is modulated (constant fan speed) to meet the
        //    heating load.

        // REFERENCES:
        // ASHRAE Systems and Equipment Handbook (SI), 1996. page 31.7

        // Using/Aliasing
        using namespace DataZoneEnergyDemands;
        using General::SolveRoot;
        using PlantUtilities::SetComponentFlowRate;

        // SUBROUTINE PARAMETER DEFINITIONS:
        int constexpr MaxIter(100); // maximum number of iterations

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int ControlNode;      // the hot water inlet node
        int InletNode;        // unit air inlet node
        int OutletNode;       // unit air outlet node
        Real64 ControlOffset; // tolerance for output control
        Real64 MaxWaterFlow;  // maximum water flow for heating or cooling [kg/sec]
        Real64 MinWaterFlow;  // minimum water flow for heating or cooling [kg/sec]
        Real64 QUnitOut;      // heating or sens. cooling provided by fan coil unit [watts]
        Real64 LatentOutput;  // Latent (moisture) add/removal rate, negative is dehumidification [kg/s]
        Real64 SpecHumOut;    // Specific humidity ratio of outlet air (kg moisture / kg moist air)
        Real64 SpecHumIn;     // Specific humidity ratio of inlet air (kg moisture / kg moist air)
        Real64 mdot;          // local temporary for fluid mass flow rate
        HVAC::FanOp fanOp;
        Real64 PartLoadFrac;
        Real64 NoOutput;
        Real64 FullOutput;
        int SolFlag; // return flag from RegulaFalsi for sensible load
        bool UnitOn;

        // initialize local variables
        QUnitOut = 0.0;
        NoOutput = 0.0;
        FullOutput = 0.0;
        LatentOutput = 0.0;
        MaxWaterFlow = 0.0;
        MinWaterFlow = 0.0;
        PartLoadFrac = 0.0;
        SolFlag = 0; // # of iterations IF positive, -1 means failed to converge, -2 means bounds are incorrect
        InletNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode;
        OutletNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode;
        ControlNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode;
        ControlOffset = state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlOffset;
        UnitOn = false;
        fanOp = state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanOp;

        if (fanOp != HVAC::FanOp::Cycling) {

            if (GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).SchedPtr) <= 0 ||
                ((GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanAvailSchedPtr) <= 0 &&
                  !state.dataHVACGlobal->TurnFansOn) ||
                 state.dataHVACGlobal->TurnFansOff)) {
                // Case 1: OFF-->unit schedule says that it it not available
                //         OR child fan in not available OR child fan not being cycled ON by sys avail manager
                //         OR child fan being forced OFF by sys avail manager
                state.dataUnitHeaters->HCoilOn = false;
                if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::WaterHeatingCoil) {
                    mdot = 0.0; // try to turn off

                    SetComponentFlowRate(state,
                                         mdot,
                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                }
                if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::SteamCoil) {
                    mdot = 0.0; // try to turn off

                    SetComponentFlowRate(state,
                                         mdot,
                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                         state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                }
                CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, QUnitOut);

            } else if ((state.dataUnitHeaters->QZnReq < SmallLoad) || state.dataZoneEnergyDemand->CurDeadBandOrSetback(ZoneNum)) {
                // Unit is available, but there is no load on it or we are in setback/deadband
                if (!state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOffNoHeating) {

                    // Case 2: NO LOAD OR COOLING/ON-OFF FAN CONTROL-->turn everything off
                    //         because there is no load on the unit heater
                    state.dataUnitHeaters->HCoilOn = false;
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::WaterHeatingCoil) {
                        mdot = 0.0; // try to turn off

                        SetComponentFlowRate(state,
                                             mdot,
                                             state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                             state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                             state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                    }
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::SteamCoil) {
                        mdot = 0.0; // try to turn off

                        SetComponentFlowRate(state,
                                             mdot,
                                             state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                             state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                             state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                    }
                    CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, QUnitOut);

                } else {
                    // Case 3: NO LOAD OR COOLING/CONTINUOUS FAN CONTROL-->let the fan
                    //         continue to run even though there is no load (air circulation)
                    // Note that the flow rates were already set in the initialization routine
                    // so there is really nothing else left to do except call the components.

                    state.dataUnitHeaters->HCoilOn = false;
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::WaterHeatingCoil) {
                        mdot = 0.0; // try to turn off

                        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum > 0) {
                            SetComponentFlowRate(state,
                                                 mdot,
                                                 state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                                 state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                                 state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                        }
                    }
                    if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type == HCoilType::SteamCoil) {
                        mdot = 0.0; // try to turn off
                        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc.loopNum > 0) {
                            SetComponentFlowRate(state,
                                                 mdot,
                                                 state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                                 state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                                 state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                        }
                    }

                    CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, QUnitOut);
                }

            } else { // Case 4: HEATING-->unit is available and there is a heating load

                switch (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type) {

                case HCoilType::WaterHeatingCoil: {

                    // On the first HVAC iteration the system values are given to the controller, but after that
                    // the demand limits are in place and there needs to be feedback to the Zone Equipment
                    if (FirstHVACIteration) {
                        MaxWaterFlow = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxHotWaterFlow;
                        MinWaterFlow = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MinHotWaterFlow;
                    } else {
                        MaxWaterFlow = state.dataLoopNodes->Node(ControlNode).MassFlowRateMaxAvail;
                        MinWaterFlow = state.dataLoopNodes->Node(ControlNode).MassFlowRateMinAvail;
                    }
                    // control water flow to obtain output matching QZnReq
                    ControlCompOutput(state,
                                      state.dataUnitHeaters->UnitHeat(UnitHeatNum).Name,
                                      state.dataUnitHeaters->cMO_UnitHeater,
                                      UnitHeatNum,
                                      FirstHVACIteration,
                                      state.dataUnitHeaters->QZnReq,
                                      ControlNode,
                                      MaxWaterFlow,
                                      MinWaterFlow,
                                      ControlOffset,
                                      state.dataUnitHeaters->UnitHeat(UnitHeatNum).ControlCompTypeNum,
                                      state.dataUnitHeaters->UnitHeat(UnitHeatNum).CompErrIndex,
                                      _,
                                      _,
                                      _,
                                      _,
                                      _,
                                      state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                    break;
                }
                case HCoilType::Electric:
                case HCoilType::Gas:
                case HCoilType::SteamCoil: {
                    state.dataUnitHeaters->HCoilOn = true;
                    CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, QUnitOut);
                    break;
                }
                default:
                    break;
                }
            }
            QUnitOut = state.dataLoopNodes->Node(OutletNode).MassFlowRate *
                       (PsyHFnTdbW(state.dataLoopNodes->Node(OutletNode).Temp, state.dataLoopNodes->Node(InletNode).HumRat) -
                        PsyHFnTdbW(state.dataLoopNodes->Node(InletNode).Temp, state.dataLoopNodes->Node(InletNode).HumRat));
            if (state.dataLoopNodes->Node(InletNode).MassFlowRateMax > 0.0) {
                state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanPartLoadRatio =
                    state.dataLoopNodes->Node(InletNode).MassFlowRate / state.dataLoopNodes->Node(InletNode).MassFlowRateMax;
            }
        } else { // OnOff fan and cycling
            if ((state.dataUnitHeaters->QZnReq < SmallLoad) || (state.dataZoneEnergyDemand->CurDeadBandOrSetback(ZoneNum)) ||
                GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).SchedPtr) <= 0 ||
                ((GetCurrentScheduleValue(state, state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanAvailSchedPtr) <= 0 &&
                  !state.dataHVACGlobal->TurnFansOn) ||
                 state.dataHVACGlobal->TurnFansOff)) {
                // Case 1: OFF-->unit schedule says that it it not available
                //         OR child fan in not available OR child fan not being cycled ON by sys avail manager
                //         OR child fan being forced OFF by sys avail manager
                PartLoadFrac = 0.0;
                state.dataUnitHeaters->HCoilOn = false;
                CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, QUnitOut, fanOp, PartLoadFrac);

                if (state.dataLoopNodes->Node(InletNode).MassFlowRateMax > 0.0) {
                    state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanPartLoadRatio =
                        state.dataLoopNodes->Node(InletNode).MassFlowRate / state.dataLoopNodes->Node(InletNode).MassFlowRateMax;
                }

            } else { // Case 4: HEATING-->unit is available and there is a heating load

                state.dataUnitHeaters->HCoilOn = true;
                UnitOn = true;

                // Find part load ratio of unit heater coils
                PartLoadFrac = 0.0;
                CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, NoOutput, fanOp, PartLoadFrac);
                if ((NoOutput - state.dataUnitHeaters->QZnReq) < SmallLoad) {
                    // Unit heater is unable to meet the load with coil off, set PLR = 1
                    PartLoadFrac = 1.0;
                    CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, FullOutput, fanOp, PartLoadFrac);
                    if ((FullOutput - state.dataUnitHeaters->QZnReq) > SmallLoad) {
                        // Unit heater full load capacity is able to meet the load, Find PLR
                        HVAC::FanOp fanOp = state.dataUnitHeaters->UnitHeat(UnitHeatNum).fanOp;

                        auto f = [&state, UnitHeatNum, FirstHVACIteration, fanOp](Real64 const PartLoadRatio) {
                            // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
                            Real64 QUnitOut; // heating provided by unit heater [watts]

                            CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, QUnitOut, fanOp, PartLoadRatio);

                            // Calculate residual based on output calculation flag
                            if (state.dataUnitHeaters->QZnReq != 0.0) {
                                return (QUnitOut - state.dataUnitHeaters->QZnReq) / state.dataUnitHeaters->QZnReq;
                            } else
                                return 0.0;
                        };

                        // Tolerance is in fraction of load, MaxIter = 30, SolFalg = # of iterations or error as appropriate
                        SolveRoot(state, 0.001, MaxIter, SolFlag, PartLoadFrac, f, 0.0, 1.0);
                    }
                }

                CalcUnitHeaterComponents(state, UnitHeatNum, FirstHVACIteration, QUnitOut, fanOp, PartLoadFrac);

            } // ...end of unit ON/OFF IF-THEN block
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).PartLoadFrac = PartLoadFrac;
            state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanPartLoadRatio = PartLoadFrac;
            state.dataLoopNodes->Node(OutletNode).MassFlowRate = state.dataLoopNodes->Node(InletNode).MassFlowRate;
        }

        // CR9155 Remove specific humidity calculations
        SpecHumOut = state.dataLoopNodes->Node(OutletNode).HumRat;
        SpecHumIn = state.dataLoopNodes->Node(InletNode).HumRat;
        LatentOutput = state.dataLoopNodes->Node(OutletNode).MassFlowRate * (SpecHumOut - SpecHumIn); // Latent rate (kg/s), dehumid = negative

        QUnitOut = state.dataLoopNodes->Node(OutletNode).MassFlowRate *
                   (PsyHFnTdbW(state.dataLoopNodes->Node(OutletNode).Temp, state.dataLoopNodes->Node(InletNode).HumRat) -
                    PsyHFnTdbW(state.dataLoopNodes->Node(InletNode).Temp, state.dataLoopNodes->Node(InletNode).HumRat));

        // Report variables...
        state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatPower = max(0.0, QUnitOut);
        state.dataUnitHeaters->UnitHeat(UnitHeatNum).ElecPower =
            state.dataFans->fans(state.dataUnitHeaters->UnitHeat(UnitHeatNum).Fan_Index)->totalPower;

        PowerMet = QUnitOut;
        LatOutputProvided = LatentOutput;
    }

    void CalcUnitHeaterComponents(EnergyPlusData &state,
                                  int const UnitHeatNum,         // Unit index in unit heater array
                                  bool const FirstHVACIteration, // flag for 1st HVAV iteration in the time step
                                  Real64 &LoadMet,               // load met by unit (watts)
                                  HVAC::FanOp const fanOp,       // fan operating mode
                                  Real64 const PartLoadRatio     // part-load ratio
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   May 2000
        //       MODIFIED       July 2012, Chandan Sharma - FSEC: Added zone sys avail managers
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine launches the individual component simulations.
        // This is called either when the unit is off to carry null conditions
        // through the unit or during control iterations to continue updating
        // what is going on within the unit.

        // METHODOLOGY EMPLOYED:
        // Simply calls the different components in order.

        // Using/Aliasing
        using HeatingCoils::SimulateHeatingCoilComponents;
        using PlantUtilities::SetComponentFlowRate;
        using SteamCoils::SimulateSteamCoilComponents;
        using WaterCoils::SimulateWaterCoilComponents;

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        Real64 AirMassFlow; // total mass flow through the unit
        Real64 CpAirZn;     // specific heat of dry air at zone conditions (zone conditions same as unit inlet)
        int HCoilInAirNode; // inlet node number for fan exit/coil inlet
        int InletNode;      // unit air inlet node
        int OutletNode;     // unit air outlet node
        Real64 QCoilReq;    // Heat addition required from an electric/gas heating coil
        Real64 mdot;        // local temporary for fluid mass flow rate

        InletNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode;
        OutletNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirOutNode;
        QCoilReq = 0.0;

        if (fanOp != HVAC::FanOp::Cycling) {
            state.dataFans->fans(state.dataUnitHeaters->UnitHeat(UnitHeatNum).Fan_Index)->simulate(state, FirstHVACIteration, _, _);

            switch (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type) {

            case HCoilType::WaterHeatingCoil: {

                SimulateWaterCoilComponents(state,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                            FirstHVACIteration,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index);
                break;
            }
            case HCoilType::SteamCoil: {

                if (!state.dataUnitHeaters->HCoilOn) {
                    QCoilReq = 0.0;
                } else {
                    HCoilInAirNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOutletNode;
                    CpAirZn = PsyCpAirFnW(state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).HumRat);
                    QCoilReq =
                        state.dataUnitHeaters->QZnReq - state.dataLoopNodes->Node(HCoilInAirNode).MassFlowRate * CpAirZn *
                                                            (state.dataLoopNodes->Node(HCoilInAirNode).Temp -
                                                             state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).Temp);
                }
                if (QCoilReq < 0.0) QCoilReq = 0.0; // a heating coil can only heat, not cool
                SimulateSteamCoilComponents(state,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                            FirstHVACIteration,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index,
                                            QCoilReq);
                break;
            }
            case HCoilType::Electric:
            case HCoilType::Gas: {

                if (!state.dataUnitHeaters->HCoilOn) {
                    QCoilReq = 0.0;
                } else {
                    HCoilInAirNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOutletNode;
                    CpAirZn = PsyCpAirFnW(state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).HumRat);
                    QCoilReq =
                        state.dataUnitHeaters->QZnReq - state.dataLoopNodes->Node(HCoilInAirNode).MassFlowRate * CpAirZn *
                                                            (state.dataLoopNodes->Node(HCoilInAirNode).Temp -
                                                             state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).Temp);
                }
                if (QCoilReq < 0.0) QCoilReq = 0.0; // a heating coil can only heat, not cool
                SimulateHeatingCoilComponents(state,
                                              state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                              FirstHVACIteration,
                                              QCoilReq,
                                              state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index);
                break;
            }
            default:
                break;
            }

            AirMassFlow = state.dataLoopNodes->Node(OutletNode).MassFlowRate;

            state.dataLoopNodes->Node(InletNode).MassFlowRate =
                state.dataLoopNodes->Node(OutletNode).MassFlowRate; // maintain continuity through unit heater

        } else { // OnOff fan cycling

            state.dataLoopNodes->Node(InletNode).MassFlowRate = state.dataLoopNodes->Node(InletNode).MassFlowRateMax * PartLoadRatio;
            AirMassFlow = state.dataLoopNodes->Node(InletNode).MassFlowRate;
            // Set the fan inlet node maximum available mass flow rates for cycling fans
            state.dataLoopNodes->Node(InletNode).MassFlowRateMaxAvail = AirMassFlow;

            if (QCoilReq < 0.0) QCoilReq = 0.0; // a heating coil can only heat, not cool
            state.dataFans->fans(state.dataUnitHeaters->UnitHeat(UnitHeatNum).Fan_Index)->simulate(state, FirstHVACIteration, _, _);

            switch (state.dataUnitHeaters->UnitHeat(UnitHeatNum).Type) {

            case HCoilType::WaterHeatingCoil: {

                if (!state.dataUnitHeaters->HCoilOn) {
                    mdot = 0.0;
                    QCoilReq = 0.0;
                } else {
                    HCoilInAirNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOutletNode;
                    CpAirZn = PsyCpAirFnW(state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).HumRat);
                    QCoilReq =
                        state.dataUnitHeaters->QZnReq - state.dataLoopNodes->Node(HCoilInAirNode).MassFlowRate * CpAirZn *
                                                            (state.dataLoopNodes->Node(HCoilInAirNode).Temp -
                                                             state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).Temp);
                    mdot = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxHotWaterFlow * PartLoadRatio;
                }
                if (QCoilReq < 0.0) QCoilReq = 0.0; // a heating coil can only heat, not cool
                SetComponentFlowRate(state,
                                     mdot,
                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                SimulateWaterCoilComponents(state,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                            FirstHVACIteration,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index,
                                            QCoilReq,
                                            fanOp,
                                            PartLoadRatio);
                break;
            }
            case HCoilType::SteamCoil: {
                if (!state.dataUnitHeaters->HCoilOn) {
                    mdot = 0.0;
                    QCoilReq = 0.0;
                } else {
                    HCoilInAirNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOutletNode;
                    CpAirZn = PsyCpAirFnW(state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).HumRat);
                    QCoilReq =
                        state.dataUnitHeaters->QZnReq - state.dataLoopNodes->Node(HCoilInAirNode).MassFlowRate * CpAirZn *
                                                            (state.dataLoopNodes->Node(HCoilInAirNode).Temp -
                                                             state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).Temp);
                    mdot = state.dataUnitHeaters->UnitHeat(UnitHeatNum).MaxHotSteamFlow * PartLoadRatio;
                }
                if (QCoilReq < 0.0) QCoilReq = 0.0; // a heating coil can only heat, not cool
                SetComponentFlowRate(state,
                                     mdot,
                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotControlNode,
                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).HotCoilOutNodeNum,
                                     state.dataUnitHeaters->UnitHeat(UnitHeatNum).HWplantLoc);
                SimulateSteamCoilComponents(state,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                            FirstHVACIteration,
                                            state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index,
                                            QCoilReq,
                                            _,
                                            fanOp,
                                            PartLoadRatio);
                break;
            }
            case HCoilType::Electric:
            case HCoilType::Gas: {

                if (!state.dataUnitHeaters->HCoilOn) {
                    QCoilReq = 0.0;
                } else {
                    HCoilInAirNode = state.dataUnitHeaters->UnitHeat(UnitHeatNum).FanOutletNode;
                    CpAirZn = PsyCpAirFnW(state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).HumRat);
                    QCoilReq =
                        state.dataUnitHeaters->QZnReq - state.dataLoopNodes->Node(HCoilInAirNode).MassFlowRate * CpAirZn *
                                                            (state.dataLoopNodes->Node(HCoilInAirNode).Temp -
                                                             state.dataLoopNodes->Node(state.dataUnitHeaters->UnitHeat(UnitHeatNum).AirInNode).Temp);
                }
                if (QCoilReq < 0.0) QCoilReq = 0.0; // a heating coil can only heat, not cool
                SimulateHeatingCoilComponents(state,
                                              state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoilName,
                                              FirstHVACIteration,
                                              QCoilReq,
                                              state.dataUnitHeaters->UnitHeat(UnitHeatNum).HCoil_Index,
                                              _,
                                              _,
                                              fanOp,
                                              PartLoadRatio);
                break;
            }
            default:
                break;
            }
            state.dataLoopNodes->Node(OutletNode).MassFlowRate =
                state.dataLoopNodes->Node(InletNode).MassFlowRate; // maintain continuity through unit heater
        }
        LoadMet = AirMassFlow * (PsyHFnTdbW(state.dataLoopNodes->Node(OutletNode).Temp, state.dataLoopNodes->Node(InletNode).HumRat) -
                                 PsyHFnTdbW(state.dataLoopNodes->Node(InletNode).Temp, state.dataLoopNodes->Node(InletNode).HumRat));
    }

    // SUBROUTINE UpdateUnitHeater

    // No update routine needed in this module since all of the updates happen on
    // the Node derived type directly and these updates are done by other routines.

    // END SUBROUTINE UpdateUnitHeater

    void ReportUnitHeater(EnergyPlusData &state, int const UnitHeatNum) // Unit index in unit heater array
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Rick Strand
        //       DATE WRITTEN   May 2000
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine needs a description.

        // METHODOLOGY EMPLOYED:
        // Needs description, as appropriate.

        // Using/Aliasing
        Real64 TimeStepSysSec = state.dataHVACGlobal->TimeStepSysSec;

        state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatEnergy = state.dataUnitHeaters->UnitHeat(UnitHeatNum).HeatPower * TimeStepSysSec;
        state.dataUnitHeaters->UnitHeat(UnitHeatNum).ElecEnergy = state.dataUnitHeaters->UnitHeat(UnitHeatNum).ElecPower * TimeStepSysSec;

        if (state.dataUnitHeaters->UnitHeat(UnitHeatNum).FirstPass) { // reset sizing flags so other zone equipment can size normally
            if (!state.dataGlobal->SysSizingCalc) {
                DataSizing::resetHVACSizingGlobals(state, state.dataSize->CurZoneEqNum, 0, state.dataUnitHeaters->UnitHeat(UnitHeatNum).FirstPass);
            }
        }
    }

} // namespace UnitHeater

} // namespace EnergyPlus
