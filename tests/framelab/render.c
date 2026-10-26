// Renders one frame through the FrameLab transform.
// Usage: render in.f32 W H out.f32 [name=value ...]
// in/out: float32 RGB, interleaved, row 0 = top.
#include "framelab_gen.c"

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage\n"); return 1; }
    int W = atoi(argv[2]), H = atoi(argv[3]);
    size_t n = (size_t)W * H;
    float *in = malloc(n * 3 * sizeof(float));
    FILE *f = fopen(argv[1], "rb");
    if (!f || fread(in, sizeof(float), n * 3, f) != n * 3) { fprintf(stderr, "read fail\n"); return 1; }
    fclose(f);
    float *r = malloc(n * 4), *g = malloc(n * 4), *b = malloc(n * 4), *out = malloc(n * 12);
    for (size_t i = 0; i < n; i++) { r[i] = in[3*i]; g[i] = in[3*i+1]; b[i] = in[3*i+2]; }
    for (int i = 5; i < argc; i++) {
        char k[128]; double v;
        if (sscanf(argv[i], "%127[^=]=%lf", k, &v) != 2 || !set_param(k, v)) { fprintf(stderr, "bad param %s\n", argv[i]); return 2; }
    }
    Tex tr = {r, W, H}, tg = {g, W, H}, tb = {b, W, H};
    long bad = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float3 c = transform(W, H, x, y, tr, tg, tb);
            size_t i = (size_t)y * W + x;
            if (!isfinite(c.x) || !isfinite(c.y) || !isfinite(c.z)) bad++;
            out[3*i] = c.x; out[3*i+1] = c.y; out[3*i+2] = c.z;
        }
    f = fopen(argv[4], "wb"); fwrite(out, sizeof(float), n * 3, f); fclose(f);
    if (bad) { fprintf(stderr, "NONFINITE %ld\n", bad); return 4; }
    return 0;
}
