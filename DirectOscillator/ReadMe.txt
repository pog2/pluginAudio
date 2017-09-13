** DirectOscillator **

Implementation of an oscillator design by direct form : by puttin to pole on the ComplexPlane.

The difference equation implemented is 
y[0] = sin(-2*theta)
y[1] = sin(-theta)
y[n+1] = -b1*y[n-1] - b2*y[n-2]

b2 = 1.0
b1 = -2cos(theta)
theta = (2*pi*f0)/fs