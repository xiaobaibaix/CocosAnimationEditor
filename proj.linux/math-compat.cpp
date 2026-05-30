extern "C" {
float __powf_finite(float x, float y) { return __builtin_powf(x, y); }
float __expf_finite(float x) { return __builtin_expf(x); }
}
