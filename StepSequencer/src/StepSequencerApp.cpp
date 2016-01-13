#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/GainNode.h"
#include "cinder/audio/Node.h"
#include "cinder/audio/Param.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/TextureFont.h"
#include "cinder/params/Params.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci::app;
using namespace ci::audio;

class Source {
public:
    
    ///////////////////////////
    // Static
    ///////////////////////////
    typedef shared_ptr<Source> SourceRef;
    static SourceRef create(float initFrequency) {
        return make_shared<Source>(initFrequency);
    }
    
    static int noteDuration;
    
    ///////////////
    // Ctr
    ///////////////
    Source(float initFrequency) : frequency(initFrequency) {
        phase = 0.0f;
        enabled = true;
        index = noteDuration; // in samples per second
        phaseInc = frequency / audio::master()->getSampleRate();
    }
    
    ///////////////
    // Get/Set
    ///////////////
    bool isEnabled() { return enabled; }
    void setEnabled(bool value) {
        enabled = value;
    }
    
    float getOutput() {
        float env = (1.0f - ((float)index / (float)noteDuration));
        return sin(phase * M_PI * 2.0f) * env;
    }
    
    bool hasData() {
        return index < noteDuration;
    }
    
    ///////////////
    // Methods
    ///////////////
    void process() {
        index++;
        phase += phaseInc;
    }
    
    void reset() {
        phase = 0.0f;
        index = noteDuration;
    }
    
    void start() {
        index = 0;
        phase = 0.0f;
    }
    
private:
    ///////////////
    // Properties
    ///////////////
    float phase;
    float phaseInc;
    float frequency;
    int index;
    bool enabled;
};

int Source::noteDuration = 22050;
typedef std::shared_ptr<Source> SourceRef;

class Sequencer : public Node {
public:
    
    /////////////////////////////////////
    // Enums
    /////////////////////////////////////
    enum class Beat {
        _1 = 1,
        _2 = 2,
        _4 = 4,
        _8 = 8,
        _16 = 16,
        _32 = 32
    };
    
    /////////////////////////////////////
    // Constructors
    /////////////////////////////////////
    
    Sequencer() : Node(Node::Format()), step(0), steps(16), tempo(120) {};
    
    /////////////////////////////////////
    // Get/Set
    /////////////////////////////////////
    int getBeatSampleLength(Beat beat) {
        if(beatSampleLengths.count(beat)) {
            return beatSampleLengths.at(beat);
        }
        return 0;
    }
    
    int getBeat(Beat beat) {
        if(sampleIndex == 0) return 1;
        int n = (sampleIndex / beatSampleLengths.at(beat));
        if(beat == Beat::_1) return 1 + n;
        return 1 + (n % (int)beat);
    }
    
    int getTempo() { return tempo; }
    void setTempo(int newTempo) {
        if(tempo != newTempo) {
            tempo = constrain(newTempo, 1, 300);
            setTimingProps();
        }
    }
    
    map<int, vector<SourceRef>> & getSources() { return sources; }
    
    int getSampleIndex() { return sampleIndex; }
    
    /////////////////////////////////////
    // Methods
    /////////////////////////////////////
    void addSource(int beat, SourceRef source) {
        if(sources.count(beat) == 0) {
            sources.insert(pair<int, vector<SourceRef>>(beat, vector<SourceRef>()));
        }
        sources.at(beat).push_back(source);
    }
    
    void reset() {
        step = 0;
        sampleIndex = 0;
    }
    
    virtual void initialize() override {
        sampleIndex = 0;
        setTimingProps();
    }
    
    virtual void process(ci::audio::Buffer * buffer) override {
        if(isEnabled()) {
            auto data = buffer->getData();
            int numFrames = buffer->getNumFrames();
            for(int i = 0; i < numFrames; i++) {
                if(fmod(sampleIndex, beatSampleLengths.at(Beat::_1)) == 0) {
                    console() << "01: " << sampleIndex << endl;
                    }
                
                if(fmod(sampleIndex, beatSampleLengths.at(Beat::_2)) == 0) {
                    console() << "\t02: " << sampleIndex << endl;
                }
                
                if(fmod(sampleIndex, beatSampleLengths.at(Beat::_4)) == 0) {
                    console() << "\t\t04: " << sampleIndex << endl;
                    
                }
                
                if(fmod(sampleIndex, beatSampleLengths.at(Beat::_8)) == 0) {
                    console() << "\t\t\t08: " << sampleIndex << endl;
                }
                
                if(fmod(sampleIndex, beatSampleLengths.at(Beat::_16)) == 0) {
                    console() << "\t\t\t\t16: " << sampleIndex << " step: " << step << endl;
                    if(sources.count(step) != 0) {
                        for(auto source : sources.at(step)) {
                            source->start();
                        }
                    }
                    
                    if(++step > steps - 1) step = 0;
                }
                
                float n = 0.0f;
                for(auto & kv : sources) {
                    for(auto & source : kv.second) {
                        if(source->hasData()) {
                            source->process();
                            if(source->isEnabled()) {
                                // Sum the outputs of the sources
                                // Apply a bit of limiting to the total, to prevent distorion
                                n += source->getOutput() * 1.0f/4.0f;
                            }
                        }
                    }
                }
                data[i] = n;
                sampleIndex++;
            }
        }
    }
                
private:
    /////////////////////////////////////
    // Properties
    /////////////////////////////////////

    int tempo;
    int sampleIndex;
    int step;
    int steps;

    map<Beat, int> beatSampleLengths;
    map<int, vector<SourceRef>> sources;
    vector<SourceRef> sourceQueue;

    /////////////////////////////////////
    // Methods
    /////////////////////////////////////
    void setTimingProps() {
        auto addBeatLength = [&](Beat beat, int duration) {
            if(beatSampleLengths.count(beat) == 0) {
                beatSampleLengths.insert(std::pair<Beat, int>(beat, duration));
            } else {
                beatSampleLengths.at(beat) = duration;
            }
            
            console() << "Sequencer.setTimingProps() :: tempo[" << tempo
            << "] :: beat[" << toString((int)beat)
            << "] :: duration[" << beatSampleLengths.at(beat)
            << "]" << endl;
        };
        
        int _32ndNoteSampleDuration = (audio::master()->getSampleRate() * 60) / (tempo * 8);
        addBeatLength(Beat::_1, _32ndNoteSampleDuration * (int)Beat::_32);
        addBeatLength(Beat::_2, _32ndNoteSampleDuration * (int)Beat::_16);
        addBeatLength(Beat::_4, _32ndNoteSampleDuration * (int)Beat::_8);
        addBeatLength(Beat::_8, _32ndNoteSampleDuration * (int)Beat::_4);
        addBeatLength(Beat::_16, _32ndNoteSampleDuration * (int)Beat::_2);
        addBeatLength(Beat::_32, _32ndNoteSampleDuration);
    }
};

typedef std::shared_ptr<Sequencer> SequencerRef;
                    
class StepSequencerApp : public App {
public:
    void setup() override;
    void draw() override;
    void togglePlayback();
    
    SequencerRef sequencer;
    audio::Context * ctx;
    audio::GainNodeRef gain;
    params::InterfaceGlRef params;
    string timeStamp;
};

void StepSequencerApp::setup() {
    // Set up audio components
    ctx = audio::master();
    sequencer = ctx->makeNode(new Sequencer);
    gain = ctx->makeNode(new audio::GainNode);
    sequencer >> gain >> ctx->getOutput();
    for(int i = 0; i < 16; i++) {
        sequencer->addSource(i, Source::create(randInt(200, 1000)));
    }
    
    gain->enable();
    ctx->enable();
    
    // Set up the Params
    params = params::InterfaceGl::create(getWindow(), "Step Sequencer App", ivec2(200, 200 ));
    function<void(int)> setter = bind(&Sequencer::setTempo, sequencer, std::placeholders::_1);
    std::function<int()> getter = bind(&Sequencer::getTempo, sequencer);
    params->addText("timestamp", "label=''");
    params->addParam("Tempo", setter, getter);
    params->addParam("Note Duration", &Source::noteDuration).min(2000).max(44100).step(100);
    params->addButton("Play/Stop", std::bind(&StepSequencerApp::togglePlayback, this));
}

void StepSequencerApp::togglePlayback() {
    if(sequencer->isEnabled()) {
        sequencer->disable();
    } else {
        for(auto kv : sequencer->getSources()) {
            for(auto source : kv.second) {
                source->reset();
            }
        }
        sequencer->reset();
        sequencer->enable();
    }
}

void StepSequencerApp::draw() {
    gl::clear( Color( 0, 0, 0 ) );
    gl::enableAlphaBlending();
    params->draw();
}

CINDER_APP(StepSequencerApp, RendererGl)
