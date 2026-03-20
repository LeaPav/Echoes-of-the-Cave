#pragma once

class Gravity {
private:
    float gravity = -9.81f; 
    float verticalVelocity = 0.0f;
    float maxVelocity = -50.0f;
    bool isFalling = true;

public:
    void update();
    void setFalling();
};