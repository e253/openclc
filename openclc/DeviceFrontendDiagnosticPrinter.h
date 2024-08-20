//===--- TextDiagnosticPrinter.h - Text Diagnostic Client -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is a concrete diagnostic client, which prints the diagnostics to
// standard error.
//
// It's modified to suppress some preprocessor errors when extracting device code from c sources
// like #include errors for c system headers.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_DEVICE_FRONTEND_DIAGNOSTIC_PRINTER_H
#define LLVM_CLANG_DEVICE_FRONTEND_DIAGNOSTIC_PRINTER_H

#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/LLVM.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include <memory>

namespace clang {
class DiagnosticOptions;
class LangOptions;
class TextDiagnostic;

class DeviceFrontendDiagnosticPrinter : public DiagnosticConsumer {
    raw_ostream& OS;
    IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts;

    /// Handle to the currently active text diagnostic emitter.
    std::unique_ptr<TextDiagnostic> TextDiag;

    /// A string to prefix to error messages.
    std::string Prefix = "Device Frontend";

    LLVM_PREFERRED_TYPE(bool)
    unsigned OwnsOutputStream : 1;

public:
    DeviceFrontendDiagnosticPrinter(raw_ostream& os, DiagnosticOptions* diags,
        bool OwnsOutputStream = false);
    ~DeviceFrontendDiagnosticPrinter() override;

    /// setPrefix - Set the diagnostic printer prefix string, which will be
    /// printed at the start of any diagnostics. If empty, no prefix string is
    /// used.
    void setPrefix(std::string Value) { Prefix = std::move(Value); }

    void BeginSourceFile(const LangOptions& LO, const Preprocessor* PP) override;
    void EndSourceFile() override;
    void HandleDiagnostic(DiagnosticsEngine::Level Level,
        const Diagnostic& Info) override;
};

} // end namespace clang

#endif