//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#define __DS2_LOG_CLASS_NAME__ "Target::Process"

#include "DebugServer2/Target/Process.h"
#include "DebugServer2/Target/POSIX/Process.h"
#include "DebugServer2/Host/POSIX/PTrace.h"
#include "DebugServer2/BreakpointManager.h"
#include "DebugServer2/Log.h"

#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <cerrno>

using ds2::Target::POSIX::Process;
using ds2::Host::ProcessSpawner;
using ds2::ErrorCode;

Process::Process() : Target::ProcessBase() {}

Process::~Process() {}

ErrorCode Process::detach() {
  prepareForDetach();
  ErrorCode error = ptrace().detach(_pid);
  if (error == kSuccess) {
    cleanup();
    _flags &= ~kFlagAttachedProcess;
  }
  return error;
}

ErrorCode Process::interrupt() {
  //
  // Depending on the passthru state of signal SIGINT, deliver either
  // SIGINT or SIGTRAP.
  //
  int signal = SIGTRAP;
  if (_passthruSignals.find(SIGINT) != _passthruSignals.end()) {
    signal = SIGINT;
  }
  return ptrace().kill(_pid, signal);
}

ErrorCode Process::terminate() {
  // We have to use SIGKILL here, for two (related) reasons:
  // (1) we want to make sure the tracee can't catch the signal when we are
  //     trying to terminate it;
  // (2) using SIGKILL allows us to send the signal with a simple `kill()`
  //     instead of having to use ptrace(PTRACE_CONT, ...)`.
  return ptrace().kill(_pid, SIGKILL);
}

bool Process::isAlive() const { return (_pid > 0 && ::kill(_pid, 0) == 0); }

ErrorCode Process::suspend() { return ptrace().suspend(_pid); }

ErrorCode Process::resume(int signal, std::set<Thread *> const &excluded) {
  ProcessInfo info;
  ErrorCode error;

  error = getInfo(info);
  if (error != kSuccess)
    return error;

  return ptrace().resume(_pid, info, signal);
}

ErrorCode Process::readMemory(Address const &address, void *data, size_t length,
                              size_t *count) {
  return ptrace().readMemory(_pid, address, data, length, count);
}

ErrorCode Process::writeMemory(Address const &address, void const *data,
                               size_t length, size_t *count) {
  return ptrace().writeMemory(_pid, address, data, length, count);
}

ErrorCode Process::wait(int *status, bool hang) {
  return ptrace().wait(_pid, hang, status);
}

ds2::Target::Process *Process::Attach(ProcessId pid) {
  if (pid <= 0)
    return nullptr;

  //
  // Create the process.
  //
  Target::Process *process = new Target::Process;

  //
  // And try to attach.
  //
  ErrorCode error = process->ptrace().attach(pid);
  if (error != kSuccess) {
    DS2LOG(Target, Error, "ptrace attach failed: %s", strerror(errno));
    delete process;
    return nullptr;
  }

  error = process->initialize(pid, kFlagAttachedProcess);
  if (error != kSuccess) {
    process->ptrace().detach(pid);
    delete process;
    return nullptr;
  }

  return process;
}

ds2::Target::Process *Process::Create(ProcessSpawner &spawner) {
  pid_t pid;

  //
  // Create the process.
  //
  Target::Process *process = new Target::Process;

  spawner.run([process](){ return process->ptrace().traceMe(true) == kSuccess; });

  pid = spawner.pid();

  //
  // Wait the process.
  //
  if (process->initialize(pid, 0) != kSuccess) {
    delete process;
    return nullptr;
  }

  //
  // Give a chance to the ptracer to set any specific options.
  //
  ErrorCode error = process->ptrace().traceThat(pid);
  if (error != kSuccess) {
    delete process;
    return nullptr;
  }

  return process;
}

void Process::resetSignalPass() {
  _passthruSignals.clear();
  _passthruSignals.insert(SIGINT);
}

void Process::setSignalPass(int signo, bool set) {
  //
  // signal 0, SIGSTOP, SIGCHLD and SIGRTMIN are specially
  // handled, so we never pass them thru.
  //
  if (signo == 0 || signo == SIGSTOP || signo == SIGCHLD
#if defined(__linux__)
      || signo == SIGRTMIN
#endif
      )
    return;

  if (set) {
    _passthruSignals.insert(signo);
  } else {
    _passthruSignals.erase(signo);
  }
}
