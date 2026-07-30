#pragma once

class ResourceService {
public:
    bool begin();
    bool filesystemReady() const { return filesystemReady_; }
    bool packReady() const { return packReady_; }

private:
    bool initialized_ = false;
    bool filesystemReady_ = false;
    bool packReady_ = false;
};
