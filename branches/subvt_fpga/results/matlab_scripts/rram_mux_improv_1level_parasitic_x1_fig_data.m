%% Format
% input size, number of fin in each transistor, delay, leakage power, dynamic power, energy per toggle;

%% RRAM MUX improved design, long wires is placed close to input inverters
rram_mux_improv_1level_parasitic_x1_0p5V = [
% input size = 2
2, 1,1.477e-11,6.537e-09,2.4255e-06,8.017e-17;
2, 2,1.4645e-11,9.779e-09,2.6955e-06,8.6845e-17;
2, 3,1.4735e-11,1.3e-08,2.9015e-06,9.3365e-17;
% input size 4
4,1,1.514e-11,1.203e-08,2.437e-06,8.25e-17;
4,2,1.4955e-11,1.792e-08,2.7295e-06,8.84e-17;
4,3,1.504e-11,2.289e-08,2.931e-06,9.5105e-17;
% input size 6
6,1,1.55e-11,1.748e-08,2.4375e-06,8.3025e-17;
6,2,1.5085e-11,2.579e-08,2.75e-06,9.011e-17;
6,3,1.539e-11,3.275e-08,2.9315e-06,9.6665e-17;
% input size 8
8,1,1.588e-11,2.292e-08,2.4455e-06,8.516e-17;
8,2,1.539e-11,3.269e-08,2.754e-06,9.1645e-17;
8,3,1.5225e-11,4.35e-08,2.97e-06,9.8625e-17;
% input size 10
10,1,1.6265e-11,2.834e-08,2.4585e-06,8.653e-17;
10,2,1.5695e-11,4.027e-08,2.772e-06,9.315e-17;
10,3,1.6e-11,5.239e-08,2.987e-06,1.0009e-16;
% input size 12
12,1,1.6655e-11,3.376e-08,2.4645e-06,8.788e-17;
12,2,1.6085e-11,4.784e-08,2.792e-06,9.4615e-17;
12,3,1.628e-11,6.218e-08,3.0195e-06,1.01925e-16;
% input size 14
14,1,1.7205e-11,3.919e-08,2.463e-06,8.9235e-17;
14,2,1.633e-11,5.541e-08,2.809e-06,9.6135e-17;
14,3,1.656e-11,7.196e-08,3.0465e-06,1.0366e-16;
% input size 16
16,1,1.7855e-11,4.584e-08,2.47e-06,9.0595e-17;
16,2,1.6565e-11,6.296e-08,2.8255e-06,9.766e-17;
16,3,1.682e-11,8.172e-08,3.072e-06,1.054e-16;
% input size 18
18,1,1.846e-11,5.136e-08,2.465e-06,9.199e-17;
18,2,1.6795e-11,7.052e-08,2.842e-06,9.923e-17;
18,3,1.6655e-11,9.147e-08,2.985e-06,1.0283e-16;
% input size 20
20,1,1.9e-11,5.684e-08,2.4715e-06,9.399e-17;
20,2,1.702e-11,7.808e-08,2.857e-06,1.0075e-16;
20,3,1.652e-11,1.012e-07,3.0305e-06,1.05195e-16;
% input size 22
22,1,1.9235e-11,6.238e-08,2.5e-06,9.4585e-17;
22,2,1.746e-11,8.562e-08,2.876e-06,1.02465e-16;
22,3,1.6765e-11,1.109e-07,3.0535e-06,1.0695e-16;
% input size 24
24,1,1.9575e-11,6.789e-08,2.503e-06,9.607e-17;
24,2,1.7695e-11,9.314e-08,2.894e-06,1.041e-16;
24,3,1.7005e-11,1.206e-07,3.0805e-06,1.087e-16;
% input size 26
26,1,1.9905e-11,7.34e-08,2.4935e-06,9.7945e-17;
26,2,1.792e-11,1.007e-07,2.911e-06,1.057e-16;
26,3,1.724e-11,1.304e-07,3.0945e-06,1.105e-16;
% input size 28
28,1,2.023e-11,7.886e-08,2.4875e-06,9.873e-17;
28,2,1.8145e-11,1.082e-07,2.9285e-06,1.0725e-16;
28,3,1.7995e-11,1.401e-07,3.2175e-06,1.158e-16;
% input size 30
30,1,2.102e-11,8.259e-08,2.5e-06,1.0066e-16;
30,2,1.8365e-11,1.157e-07,2.9445e-06,1.087e-16;
30,3,1.837e-11,1.498e-07,3.237e-06,1.174e-16;
% input size 32
32,1,2.137e-11,8.797e-08,2.5055e-06,1.0212e-16;
32,2,1.8635e-11,1.232e-07,2.953e-06,1.102e-16;
32,3,1.856e-11,1.595e-07,3.2595e-06,1.1915e-16;
];

rram_mux_improv_1level_parasitic_x1_0p6V = [
% input size = 2
2,1,1.194e-11,6.78e-09,4.5315e-06,1.211e-16;
2,2,1.114e-11,1.098e-08,5.081e-06,1.3095e-16;
2,3,1.114e-11,1.52e-08,5.426e-06,1.4015e-16;
% input size 4
4,1,1.2255e-11,1.22e-08,4.527e-06,1.23e-16;
4,2,1.1405e-11,1.92e-08,5.1035e-06,1.3305e-16;
4,3,1.1315e-11,2.627e-08,5.4565e-06,1.4235e-16;
% input size 6
6,1,1.245e-11,1.759e-08,4.5345e-06,1.2525e-16;
6,2,1.167e-11,2.739e-08,5.136e-06,1.35e-16;
6,3,1.149e-11,3.73e-08,5.485e-06,1.4475e-16;
% input size 8
8,1,1.275e-11,2.296e-08,4.5115e-06,1.27e-16;
8,2,1.1935e-11,3.555e-08,5.151e-06,1.368e-16;
8,3,1.1665e-11,4.831e-08,5.51e-06,1.472e-16;
% input size 10
10,1,1.3025e-11,2.831e-08,4.517e-06,1.2885e-16;
10,2,1.218e-11,4.37e-08,5.1735e-06,1.3875e-16;
10,3,1.185e-11,5.931e-08,5.6055e-06,1.4895e-16;
% input size 12
12,1,1.3265e-11,3.365e-08,4.523e-06,1.306e-16;
12,2,1.2415e-11,5.183e-08,5.194e-06,1.408e-16;
12,3,1.2055e-11,7.029e-08,5.6385e-06,1.51e-16;
% input size 14
14,1,1.3605e-11,3.898e-08,4.53e-06,1.325e-16;
14,2,1.2645e-11,5.995e-08,5.2105e-06,1.4275e-16;
14,3,1.227e-11,8.126e-08,5.679e-06,1.532e-16;
% input size 16
16,1,1.413e-11,4.43e-08,4.6585e-06,1.357e-16;
16,2,1.286e-11,6.857e-08,5.2275e-06,1.4475e-16;
16,3,1.2485e-11,9.221e-08,5.718e-06,1.5535e-16;
% input size 18
18,1,1.4645e-11,4.961e-08,4.5365e-06,1.363e-16;
18,2,1.306e-11,7.617e-08,5.235e-06,1.471e-16;
18,3,1.269e-11,1.032e-07,5.756e-06,1.5755e-16;
% input size 20
20,1,1.497e-11,5.491e-08,4.5415e-06,1.382e-16;
20,2,1.3235e-11,8.427e-08,5.2675e-06,1.492e-16;
20,3,1.29e-11,1.141e-07,5.7915e-06,1.598e-16;
% input size 22
22,1,1.525e-11,6.02e-08,4.571e-06,1.4095e-16;
22,2,1.3405e-11,9.236e-08,5.2925e-06,1.512e-16;
22,3,1.3095e-11,1.25e-07,5.828e-06,1.6195e-16;
% input size 24
24,1,1.503e-11,6.549e-08,4.5925e-06,1.418e-16;
24,2,1.3405e-11,1.004e-07,5.3555e-06,1.527e-16;
24,3,1.328e-11,1.36e-07,5.864e-06,1.642e-16;
% input size 26
26,1,1.5565e-11,7.078e-08,4.594e-06,1.4355e-16;
26,2,1.3745e-11,1.085e-07,5.337e-06,1.551e-16;
26,3,1.3465e-11,1.469e-07,5.8985e-06,1.6645e-16;
% input size 28
28,1,1.578e-11,7.606e-08,4.5935e-06,1.454e-16;
28,2,1.3895e-11,1.166e-07,5.359e-06,1.571e-16;
28,3,1.364e-11,1.578e-07,5.93e-06,1.686e-16;
% input size 30
30,1,1.601e-11,8.19e-08,4.584e-06,1.473e-16;
30,2,1.4045e-11,1.247e-07,5.38e-06,1.591e-16;
30,3,1.381e-11,1.687e-07,5.9585e-06,1.709e-16;
% input size 32
32,1,1.624e-11,8.719e-08,4.5995e-06,1.4915e-16;
32,2,1.419e-11,1.327e-07,5.3995e-06,1.6115e-16;
32,3,1.397e-11,1.796e-07,5.9835e-06,1.7315e-16;
];

rram_mux_improv_1level_parasitic_x1_0p7V = [
% input size 2
2,1,1.043e-11,7.665e-09,7.219e-06,1.7395e-16;
2,2,9.6425e-12,1.3e-08,8.126e-06,1.8685e-16;
2,3,9.64e-12,1.847e-08,8.687e-06,1.9875e-16;
% input size 4
4,1,1.0485e-11,1.38e-08,7.436e-06,1.7975e-16;
4,2,9.823e-12,2.245e-08,8.117e-06,1.896e-16;
4,3,9.7645e-12,3.139e-08,8.758e-06,2.0175e-16;
% input size 6
6,1,1.097e-11,1.979e-08,7.28e-06,1.788e-16;
6,2,9.9745e-12,3.188e-08,8.214e-06,1.9255e-16;
6,3,9.875e-12,4.442e-08,8.817e-06,2.0465e-16;
% input size 8
8,1,1.135e-11,2.531e-08,7.299e-06,1.8155e-16;
8,2,1.01165e-11,4.13e-08,8.258e-06,1.9525e-16;
8,3,1.0011e-11,5.744e-08,8.968e-06,2.0975e-16;
% input size 10
10,1,1.1715e-11,3.174e-08,7.298e-06,1.841e-16;
10,2,1.03e-11,5.07e-08,8.303e-06,1.98e-16;
10,3,1.011e-11,7.044e-08,8.9395e-06,2.1045e-16;
% input size 12
12,1,1.2075e-11,3.771e-08,7.3055e-06,1.864e-16;
12,2,1.049e-11,6.01e-08,8.3405e-06,2.0035e-16;
12,3,1.0265e-11,8.456e-08,8.8515e-06,2.137e-16;
% input size 14
14,1,1.2435e-11,4.286e-08,7.2895e-06,1.8935e-16;
14,2,1.0695e-11,6.949e-08,8.361e-06,2.0295e-16;
14,3,1.039e-11,9.773e-08,9.049e-06,2.168e-16;
% input size 16
16,1,1.278e-11,4.87e-08,7.28e-06,1.9205e-16;
16,2,1.0955e-11,7.887e-08,8.3965e-06,2.0565e-16;
16,3,1.045e-11,1.109e-07,9.255e-06,2.2155e-16;
% input size 18
18,1,1.2865e-11,5.453e-08,7.484e-06,1.9785e-16;
18,2,1.1205e-11,8.944e-08,8.4405e-06,2.0795e-16;
18,3,1.063e-11,1.24e-07,9.1835e-06,2.2215e-16;
% input size 20
20,1,1.3325e-11,6.036e-08,7.3225e-06,1.972e-16;
20,2,1.144e-11,9.894e-08,8.4655e-06,2.105e-16;
20,3,1.084e-11,1.372e-07,9.226e-06,2.25e-16;
% input size 22
22,1,1.3575e-11,6.618e-08,7.3505e-06,1.997e-16;
22,2,1.137e-11,1.084e-07,8.6775e-06,2.1585e-16;
22,3,1.094e-11,1.503e-07,9.42e-06,2.304e-16;
% input size 24
24,1,1.38e-11,7.2e-08,7.359e-06,2.0275e-16;
24,2,1.1685e-11,1.164e-07,8.5005e-06,2.159e-16;
24,3,1.124e-11,1.634e-07,9.3265e-06,2.3055e-16;
% input size 26
26,1,1.402e-11,7.782e-08,7.36e-06,2.0555e-16;
26,2,1.187e-11,1.257e-07,8.532e-06,2.185e-16;
26,3,1.1425e-11,1.765e-07,9.382e-06,2.3335e-16;
% input size 28
28,1,1.4235e-11,8.364e-08,7.3575e-06,2.0805e-16;
28,2,1.205e-11,1.351e-07,8.6145e-06,2.2075e-16;
28,3,1.161e-11,1.897e-07,9.4325e-06,2.361e-16;
% input size 30
30,1,1.4445e-11,8.945e-08,7.308e-06,2.1065e-16;
30,2,1.2235e-11,1.444e-07,8.6425e-06,2.234e-16;
30,3,1.179e-11,2.028e-07,9.4775e-06,2.39e-16;
% input size 32
32,1,1.465e-11,9.526e-08,7.3645e-06,2.1295e-16;
32,2,1.241e-11,1.538e-07,8.6595e-06,2.2605e-16;
32,3,1.1695e-11,2.159e-07,9.666e-06,2.448e-16;
];
