"""Colour space data and transfer functions for the CineCore LUT baker.

EVERY DaVinci Wide Gamut number here is used verbatim as supplied from
Blackmagic Design's published specification. Nothing is derived, fitted or
inferred. The Rec.709 matrices are the published ITU-R BT.709 / D65 values,
required because the source LUTs are Rec.709 LUTs.
"""
import math

# --- DaVinci Wide Gamut, official published values -------------------------
DWG_PRIMARIES = {'R': (0.8000, 0.3130), 'G': (0.1682, 0.9877),
                 'B': (0.0790, -0.1155), 'W': (0.3127, 0.3290)}

DWG_TO_XYZ = [[ 0.70062239,  0.14877482,  0.10105872],
              [ 0.27411851,  0.87363190, -0.14775041],
              [-0.09896291, -0.13789533,  1.32591599]]

XYZ_TO_DWG = [[ 1.51667204, -0.28147805, -0.14696363],
              [-0.46491710,  1.25142378,  0.17488461],
              [ 0.06484905,  0.10913934,  0.76141462]]

# --- Rec.709, published ITU-R BT.709 with a D65 white ----------------------
# Both spaces are D65, so no chromatic adaptation is involved anywhere.
REC709_TO_XYZ = [[0.4123907992659595, 0.3575843393838780, 0.1804807884018343],
                 [0.2126390058715104, 0.7151686787677559, 0.0721923153607337],
                 [0.0193308187155918, 0.1191947797946259, 0.9505321522496608]]

XYZ_TO_REC709 = [[ 3.2409699419045213, -1.5373831775700935, -0.4986107602930033],
                 [-0.9692436362808798,  1.8759675015077206,  0.0415550574071756],
                 [ 0.0556300796969936, -0.2039769588889765,  1.0569715142428786]]

# --- DaVinci Intermediate, constants confirmed from Blackmagic docs ---------
DI_A, DI_B, DI_C, DI_M = 0.0075, 7.0, 0.07329248, 10.44426855
DI_LIN_CUT, DI_LOG_CUT = 0.00262409, 0.02740668

def di_to_lin(y):
    return y / DI_M if y <= DI_LOG_CUT else 2.0 ** (y / DI_C - DI_B) - DI_A

def lin_to_di(x):
    return x * DI_M if x <= DI_LIN_CUT else DI_C * (math.log2(max(x + DI_A, 1e-12)) + DI_B)

# --- Rec.709 transfer function, ITU-R BT.709 OETF --------------------------
def rec709_encode(L):
    if L < 0.018: return 4.5 * L
    return 1.099 * (L ** 0.45) - 0.099

def rec709_decode(V):
    if V < 0.081: return V / 4.5
    return ((V + 0.099) / 1.099) ** (1.0 / 0.45)

# --- pure power alternative, for the gamma question ------------------------
def gamma_encode(L, g=2.4): return max(L, 0.0) ** (1.0 / g)
def gamma_decode(V, g=2.4): return max(V, 0.0) ** g

def mv(M, v):
    return [M[i][0]*v[0] + M[i][1]*v[1] + M[i][2]*v[2] for i in range(3)]

def mm(A, B):
    return [[sum(A[i][k]*B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
