#!/bin/bash
# Verification script to ensure Core directory has no JUCE dependencies
# Run this before committing: ./verify_core_purity.sh

echo "Verifying Core directory has no JUCE dependencies..."

# Search for JUCE includes and types in Core directory
JUCE_MATCHES=$(find Source/Core -type f \( -name "*.cpp" -o -name "*.h" \) ! -name "CORE_RULES.md" -exec grep -l "juce::\|#include.*juce" {} \; 2>/dev/null)

if [ -z "$JUCE_MATCHES" ]; then
    echo "✅ SUCCESS: No JUCE dependencies found in Core directory"
    exit 0
else
    echo "❌ ERROR: JUCE dependencies found in Core directory!"
    echo ""
    echo "Files with JUCE dependencies:"
    echo "$JUCE_MATCHES"
    echo ""
    echo "Please remove all JUCE dependencies from Core files."
    echo "See Source/Core/CORE_RULES.md for guidelines."
    exit 1
fi


