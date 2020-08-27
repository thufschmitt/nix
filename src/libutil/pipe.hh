#pragma once

#include <memory>

#include "types.hh"
#include "util.hh"
#include "serialise.hh"

namespace nix::pipe {

class Pipe {
public:
    virtual size_t write (const unsigned char* data, size_t len) = 0;
    virtual size_t read (size_t max, unsigned char* data) = 0;

};

class BufferedPipe: Pipe {
    std::vector<unsigned char> buffer;

public:
    bool isEmpty();

    size_t write (const unsigned char* data, size_t len) override;
    size_t read (size_t max, unsigned char* data) override;

    void pull(Pipe & source);
    void push(Pipe & sink);
};

class CombinedPipe: Pipe {
    Pipe & first, & second;
    BufferedPipe buffer;

public:

    CombinedPipe(Pipe & first, Pipe & second)
        : first(first), second(second) {};
    size_t write (const unsigned char* data, size_t len) override;
    size_t read (size_t max, unsigned char* data) override;
};

struct SourcePipe : Source {
    SourcePipe(Pipe & innerPipe) : innerPipe(innerPipe) {};
    Pipe & innerPipe;
    size_t read(unsigned char* data, size_t len) override {
        return innerPipe.read(len, data);
    }
};

// XXX: This assumes that `innerPipe.write` will always write everything.
// Might use a `BufferPipe` in the middle to alleviate that
struct SinkPipe : Sink {
    SinkPipe(Pipe & innerPipe) : innerPipe(innerPipe) {};
    Pipe & innerPipe;
    virtual void operator () (const unsigned char* data, size_t len) override {
        auto written = innerPipe.write(data, len);
        if (written < len) throw Error("Couldn't write everything into the pipe");
    }
};

/**
 * Pipe the output of the given source into the pipe
 */
/* Source & pipeInto(Source & source, Pipe & pipe); */

class PipedSource : Source {
    Source & producer;
    Pipe & pipe;
public:
    PipedSource(Source & producer, Pipe & pipe) : producer(producer), pipe(pipe) { }
    size_t read (unsigned char* data, size_t len) override;
};

}
