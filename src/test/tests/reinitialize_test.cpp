/*
 * Copyright 2020 McGraw-Hill Education. All rights reserved. No reproduction or distribution without the prior written consent of McGraw-Hill Education.
 */
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "callback.h"
#include "device.h"
#include "device_regs.h"
#include "func_type.h"
#include "intex.h"
#include "logger.h"
#include "printer.h"
#include "simulator.h"
#include "state.h"

using namespace lc3::core;

namespace
{
    #define CHECK(condition) \
        do { \
            if(! (condition)) { \
                std::cerr << __FILE__ << ":" << __LINE__ << ": check failed: " #condition "\n"; \
                return false; \
            } \
        } while(false)

    class BufferedInputter : public lc3::utils::IInputter
    {
    public:
        explicit BufferedInputter(std::string input) : input(std::move(input)), pos(0) {}

        virtual void beginInput(void) override {}
        virtual bool getChar(char & c) override
        {
            if(pos == input.size()) {
                return false;
            }

            c = input[pos++];
            return true;
        }
        virtual void endInput(void) override {}
        virtual bool hasRemaining(void) const override { return pos < input.size(); }

    private:
        std::string input;
        std::size_t pos;
    };

    class ResetCountingDevice : public IDevice
    {
    public:
        ResetCountingDevice(void) : reset_count(0) {}

        virtual void reset(void) override { ++reset_count; }
        virtual std::pair<uint16_t, PIMicroOp> read(uint16_t) override
        { return std::make_pair(0, nullptr); }
        virtual PIMicroOp write(uint16_t, uint16_t) override { return nullptr; }
        virtual std::vector<uint16_t> getAddrMap(void) const override { return { 0xFE10, 0xFE11 }; }
        virtual std::string getName(void) const override { return "ResetCountingDevice"; }

        uint32_t reset_count;
    };

    class NullPrinter : public lc3::utils::IPrinter
    {
    public:
        virtual void setColor(lc3::utils::PrintColor) override {}
        virtual void print(std::string const &) override {}
        virtual void newline(void) override {}
    };

    bool testMachineStateReset(void)
    {
        MachineState state;
        std::shared_ptr<ResetCountingDevice> device = std::make_shared<ResetCountingDevice>();
        state.registerDeviceReg(0xFE10, device);
        state.registerDeviceReg(0xFE11, device);

        state.writePC(0x3000);
        state.writeIR(0x1234);
        state.writeSSP(0x4321);
        state.writeReg(0, 0xABCD);
        state.writePSR(0x8704);
        state.writeMCR(0x8000);
        state.enqueueInterrupt(InterruptType::KEYBOARD);
        state.pushFuncTraceType(FuncType::INTERRUPT);
        state.addPendingCallback(CallbackType::INT_ENTER);

        state.reinitialize();

        CHECK(state.readPC() == 0);
        CHECK(state.readIR() == 0);
        CHECK(state.readDecodedIR() == nullptr);
        CHECK(state.readSSP() == 0);
        CHECK(state.readReg(0) == 0);
        CHECK(state.readPSR() == 0);
        CHECK(state.readMCR() == 0);
        CHECK(state.peekInterrupt() == InterruptType::INVALID);
        CHECK(state.peekFuncTraceType() == FuncType::INVALID);
        CHECK(state.getPendingCallbacks().empty());
        CHECK(state.isFirstInit());
        CHECK(device->reset_count == 1);

        return true;
    }

    bool testKeyboardReset(void)
    {
        BufferedInputter inputter("a");
        KeyboardDevice keyboard(inputter);

        keyboard.tick();
        CHECK((keyboard.read(KBSR).first & 0x8000) != 0);
        CHECK(keyboard.read(KBDR).first == static_cast<uint16_t>('a'));

        keyboard.reset();

        CHECK(keyboard.read(KBSR).first == 0);
        CHECK(keyboard.read(KBDR).first == 0);
        keyboard.tick();
        CHECK(keyboard.read(KBSR).first == 0);

        return true;
    }

    bool testDisplayReset(void)
    {
        NullPrinter printer;
        lc3::utils::Logger logger(printer, 0);
        DisplayDevice display(logger);

        display.write(DSR, 0x4000);
        display.tick();
        CHECK(display.read(DSR).first == 0xC000);

        display.reset();

        CHECK(display.read(DSR).first == 0);

        return true;
    }

    bool testImmediateAsyncInterrupt(void)
    {
        NullPrinter printer;
        lc3::utils::NullInputter inputter;
        Simulator simulator(printer, inputter, 0);

        // BRnzp #-1 keeps the machine running until the asynchronous interrupt is observed.
        simulator.getMachineState().writeMem(RESET_PC, 0x0FFF);

        std::thread worker([&simulator]() { simulator.simulate(); });
        simulator.asyncInterrupt();
        worker.join();

        return true;
    }
}

int main(void)
{
    if(! testMachineStateReset() || ! testKeyboardReset() || ! testDisplayReset() ||
        ! testImmediateAsyncInterrupt())
    {
        return 1;
    }

    return 0;
}
