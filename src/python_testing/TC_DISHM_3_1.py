#
#    Copyright (c) 2023 Project CHIP Authors
#    All rights reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

import logging

import chip.clusters as Clusters
from chip.clusters.Types import NullValue
from chip.interaction_model import Status
import chip.tlv
from matter_testing_support import MatterBaseTest, async_test_body, default_matter_test_main, type_matches
from mobly import asserts


class TC_DISHM_3_1(MatterBaseTest):

    async def read_mode_attribute_expect_success(self, endpoint, attribute):
        cluster = Clusters.Objects.DishwasherMode
        return await self.read_single_attribute_check_success(endpoint=endpoint, cluster=cluster, attribute=attribute)

    async def send_change_to_mode_cmd(self, newMode) -> Clusters.Objects.DishwasherMode.Commands.ChangeToModeResponse:
        ret = await self.send_single_cmd(cmd=Clusters.Objects.DishwasherMode.Commands.ChangeToMode(newMode=newMode), endpoint=self.endpoint)
        asserts.assert_true(type_matches(ret, Clusters.Objects.DishwasherMode.Commands.ChangeToModeResponse),
                            "Unexpected return type for ChangeToMode")
        return ret

    async def write_on_mode(self, newMode):
        ret = await self.default_controller.WriteAttribute(self.dut_node_id, [(self.endpoint, Clusters.DishwasherMode.Attributes.OnMode(newMode))])
        asserts.assert_equal(ret[0].Status, Status.Success, "Writing to OnMode failed")

    async def send_on_command_expect_success(self) -> Clusters.Objects.OnOff.Commands.On:
        ret = await self.send_single_cmd(cmd=Clusters.Objects.OnOff.Commands.On(), endpoint=self.endpoint)
        return ret

    async def send_off_command_expect_success(self) -> Clusters.Objects.OnOff.Commands.Off:
        ret = await self.send_single_cmd(cmd=Clusters.Objects.OnOff.Commands.Off(), endpoint=self.endpoint)
        return ret

    async def check_for_valid_mode(self, endpoint, mode):
        attr = Clusters.DishwasherMode.Attributes.SupportedModes
        supported_modes = await self.read_mode_attribute_expect_success(endpoint=endpoint, attribute=attr)

        if mode == NullValue:
            logging.info("Mode is NULL")
            return False

        mode_found = False

        for m in supported_modes:
            if m.mode == mode:
                mode_found = True

        return mode_found

    async def check_preconditions(self, endpoint):
        logging.info("Checking preconditions...")
        # Check to see if both the OnOff and DishwasherMode clusters are available on the same endpoint
        cluster = Clusters.Objects.OnOff
        attr = Clusters.OnOff.Attributes.OnOff
        onOff = await self.read_single_attribute_check_success(endpoint=endpoint, cluster=cluster, attribute=attr)
        logging.info("OnOff: %s" % (onOff))

        # Verify that OnMode is non-null and a mode value from the supported modes attribute
        cluster = Clusters.Objects.DishwasherMode
        attr = Clusters.DishwasherMode.Attributes.OnMode
        onMode = await self.read_single_attribute_check_success(endpoint=endpoint, cluster=cluster, attribute=attr)
        logging.info("OnMode: %s" % (onMode))

        cluster = Clusters.Objects.DishwasherMode
        attr = Clusters.DishwasherMode.Attributes.SupportedModes
        supported_modes = await self.read_mode_attribute_expect_success(endpoint=endpoint, attribute=attr)

        supported_modes_dut = []
        for m in supported_modes:
            if m.mode in supported_modes_dut:
                asserts.fail("SupportedModes must have unique mode values!")
            else:
                supported_modes_dut.append(m.mode)

        if onMode != NullValue and onMode in supported_modes_dut:
            return True

        return False

    @async_test_body
    async def test_TC_DISHM_3_1(self):
        # Adding endpoint here to avoid definition in command line
        self.endpoint = self.user_params.get("endpoint", 1)
        logging.info("This test expects to find this cluster on endpoint 1")

        asserts.assert_true(self.check_pics("DISHM.S.A0000"), "DISHM.S.A0000 must be supported")
        asserts.assert_true(self.check_pics("DISHM.S.A0001"), "DISHM.S.A0001 must be supported")
        asserts.assert_true(self.check_pics("DISHM.S.C00.Rsp"), "DISHM.S.C00.Rsp must be supported")
        asserts.assert_true(self.check_pics("DISHM.S.C01.Tx"), "DISHM.S.C01.Tx must be supported")

        if not self.check_pics("DISHM.S.A0003"):
            logging.info("Test skipped because PICS DISHM.S.A0003 (OnMode) is not set")
            return

        if not self.check_pics("DISHM.S.F00"):
            logging.info("Test skipped because PICS DISHM.S.F00 (DepOnOff) is not set")
            return

        ret = await self.check_preconditions(self.endpoint)
        asserts.assert_true(ret, "invalid preconditions - OnMode is null or not set to a mode in the Supported Modes attribute")

        attributes = Clusters.DishwasherMode.Attributes

        from enum import Enum

        class CommonCodes(Enum):
            SUCCESS = 0x00
            UNSUPPORTED_MODE = 0x01
            GENERIC_FAILURE = 0x02

        self.print_step(1, "Commissioning, already done")

        self.print_step(2, "Read OnMode attribute")
        # null check completed in precondition check
        on_mode_dut = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.OnMode)

        self.print_step(3, "Read CurrentMode attribute")
        old_current_mode_dut = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.CurrentMode)
        mode_check = await self.check_for_valid_mode(endpoint=self.endpoint, mode=old_current_mode_dut)
        asserts.assert_true(mode_check, "Current mode is NULL or is not a supported mode")

        if old_current_mode_dut == on_mode_dut:

            self.print_step(4, "Read SupportedModes attribute")
            supported_modes_dut = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.SupportedModes)
            asserts.assert_true(isinstance(supported_modes_dut, list), "SupportedModes must be a list")
            asserts.assert_greater_equal(len(supported_modes_dut), 2, "SupportedModes must have at least two entries!")
            asserts.assert_true(isinstance(
                supported_modes_dut[0], chip.clusters.Objects.DishwasherMode.Structs.ModeOptionStruct), "SupportedModes must must contain ModeOptionStructs")

            for m in supported_modes_dut:
                if m.mode != on_mode_dut:
                    new_mode_th = m.mode
                    break

            self.print_step(5, "Send ChangeToMode command with NewMode set to %d" % (new_mode_th))
            ret = await self.send_change_to_mode_cmd(newMode=new_mode_th)
            asserts.assert_true(ret.status == CommonCodes.SUCCESS.value, "Changing the mode should succeed")

        self.print_step(6, "Send Off command")
        ret = await self.send_off_command_expect_success()

        self.print_step(7, "Send On command")
        ret = await self.send_on_command_expect_success()

        self.print_step(8, "Read CurrentMode attribute")
        current_mode = await self.read_mode_attribute_expect_success(endpoint=self.endpoint, attribute=attributes.CurrentMode)
        asserts.assert_true(on_mode_dut == current_mode, "CurrentMode must match OnMode after a power cycle")


if __name__ == "__main__":
    default_matter_test_main()
