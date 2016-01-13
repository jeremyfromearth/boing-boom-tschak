#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/GainNode.h"
#include "cinder/audio/GenNode.h"
#include "cinder/audio/Node.h"

using namespace std;

using namespace ci;
using namespace ci::app;
using namespace cinder::audio;

class AutoPan : public Node {
public:
    AutoPan(const Format &format = Format()) : Node(format) {};
    
    Param * freq;
    Param * mix;
    
protected:
    float phase;
    virtual void initialize() override {
        phase = 0.0f;
        freq = new Param(this, 2.1f);
        mix = new Param(this, 1.0f);
    };
    
    virtual void process(ci::audio::Buffer * buffer) override {
        int numFrames = buffer->getNumFrames();
        int numChannels = buffer->getNumChannels();
        float phaseInc = (freq->getValue() / getSampleRate()) * (float)M_PI * 2.0f;
        auto data = buffer->getData();
        for(int i = 0; i < numFrames; i++) {
            phase = fmodf(phase + phaseInc, 2.0f * M_PI);
            float pan = abs(sin(phase)) * mix->getValue();
            for(int j = 0; j < numChannels; j++) {
                if(j == 1) pan = 1 - pan;
                int index = j * numFrames + i;
                float initial = data[index];
                data[index] *= pan;
                data[index] += initial * (1.0f - mix->getValue());
            }
        }
    }
};

typedef std::shared_ptr<AutoPan> AutoPanRef;

class AutoPanNodeApp : public App {
public:
    void setup() override;
    void draw() override;
    
    audio::Context * ctx;
    AutoPanRef pan;
    audio::GainNodeRef gain;
    audio::GenTriangleNodeRef osc;
};

void AutoPanNodeApp::setup() {
    ctx = audio::master();
    
    Node::Format f;
    f.setChannels(2);
    osc = ctx->makeNode(new GenTriangleNode(f));
    osc->setFreq(220);
    pan = ctx->makeNode(new AutoPan(f));
    gain = ctx->makeNode(new audio::GainNode);
    osc >> pan >> gain >> ctx->getOutput();
    pan->enable();
    osc->enable();
    ctx->enable();
}

void AutoPanNodeApp::draw() {
    gl::clear( Color( 0, 0, 0 ) );
}

CINDER_APP(AutoPanNodeApp, RendererGl)
