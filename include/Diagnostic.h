#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

struct SourceLocation {
    std::string filename;
    int line = 1;
    int column = 1;

    std::string toString() const {
        if (filename.empty()) {
            return std::to_string(line) + ":" + std::to_string(column);
        }
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

enum class DiagnosticLevel {
    Note,
    Warning,
    Error
};

struct Diagnostic {
    DiagnosticLevel level;
    SourceLocation location;
    std::string message;
};

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(std::ostream& out = std::cerr) : outStream(out) {}

    void setSource(const std::string& source, const std::string& filename = "<input>") {
        sourceCode = source;
        currentFilename = filename;
        lines.clear();
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }
    }

    void report(DiagnosticLevel level, const SourceLocation& loc, const std::string& message) {
        diagnostics.push_back({level, loc, message});
        if (level == DiagnosticLevel::Error) {
            errorCount++;
        } else if (level == DiagnosticLevel::Warning) {
            warningCount++;
        }

        if (suppressOutput) return;

        outStream << loc.toString() << ": ";
        switch (level) {
            case DiagnosticLevel::Note:    outStream << "note: "; break;
            case DiagnosticLevel::Warning: outStream << "warning: "; break;
            case DiagnosticLevel::Error:   outStream << "error: "; break;
        }
        outStream << message << "\n";

        // Print source line preview if available
        if (loc.line > 0 && loc.line <= static_cast<int>(lines.size())) {
            const std::string& srcLine = lines[loc.line - 1];
            outStream << "    " << srcLine << "\n";
            outStream << "    ";
            for (int i = 1; i < loc.column; ++i) {
                outStream << (srcLine[i - 1] == '\t' ? '\t' : ' ');
            }
            outStream << "^\n";
        }
    }

    void error(const SourceLocation& loc, const std::string& message) {
        report(DiagnosticLevel::Error, loc, message);
    }

    void warning(const SourceLocation& loc, const std::string& message) {
        report(DiagnosticLevel::Warning, loc, message);
    }

    void note(const SourceLocation& loc, const std::string& message) {
        report(DiagnosticLevel::Note, loc, message);
    }

    bool hasErrors() const { return errorCount > 0; }
    int getErrorCount() const { return errorCount; }
    int getWarningCount() const { return warningCount; }

    const std::vector<Diagnostic>& getDiagnostics() const { return diagnostics; }
    void clear() {
        diagnostics.clear();
        errorCount = 0;
        warningCount = 0;
    }

    void setSuppressOutput(bool suppress) { suppressOutput = suppress; }

private:
    std::ostream& outStream;
    std::string sourceCode;
    std::string currentFilename;
    std::vector<std::string> lines;
    std::vector<Diagnostic> diagnostics;
    int errorCount = 0;
    int warningCount = 0;
    bool suppressOutput = false;
};

#endif // DIAGNOSTIC_H
