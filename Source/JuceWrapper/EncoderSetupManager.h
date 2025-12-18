#pragma once

class Op1CloneAudioProcessorEditor;

// Manager class to handle all encoder setup and callbacks
// Extracted from PluginEditor to comply with 500-line file rule
class EncoderSetupManager {
public:
    EncoderSetupManager(Op1CloneAudioProcessorEditor* editor);
    
    // Setup all 8 encoders with their callbacks
    void setupEncoders();
    
private:
    Op1CloneAudioProcessorEditor* editor;
    
    // Helper methods for each encoder group
    void setupEncoder1();
    void setupEncoder2();
    void setupEncoder3();
    void setupEncoder4();
    void setupEncoder5();
    void setupEncoder6();
    void setupEncoder7();
    void setupEncoder8();
};

