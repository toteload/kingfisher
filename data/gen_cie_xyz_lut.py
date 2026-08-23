import csv, math

def print_c_lut(cols):
    [_, xs, ys, zs] = cols

    def make_coefficients_string(cs):
        ds = []
        for i in range((len(cs) + 1) // 2):
            s = [float(c) for c in cs[i*2:(i+1)*2]]
            s = sum(s) / len(s)
            ds.append(f'{s:.12f}f');

        ds += (256 - len(ds)) * ['{:.12f}f'.format(0)]

        out = ''
        for i in range((len(ds) + 3) // 4):
            ss = ds[i*4:(i+1)*4]
            out += '  ' + ', '.join(ss) + ',\n'

        return out

    out_x = make_coefficients_string(xs)
    out_y = make_coefficients_string(ys)
    out_z = make_coefficients_string(zs)

    code = '''// This file was generated with 'gen_cie_xyz_lut.py'
// The coefficients are based on the data of 'CIE_xyz_1931_2deg.csv' from
// https://cie.co.at/datatable/cie-1931-colour-matching-functions-2-degree-observer
// Data corresponds to the XYZ tristimulus value for wavelengths of length.
// Data starts at 360 nm and increases in steps of 2 nm up to 830 nm.
// There are 236 significant values, but the data has been extended with 0.0f
// to make the lut 256 entries long.

float const cie_xyz_x[256] = {{
{xs}}};

float const cie_xyz_y[256] = {{
{ys}}};

float const cie_xyz_z[256] = {{
{zs}}};
    '''

    print(code.format(xs=out_x, ys=out_y, zs=out_z))

def print_ppm(rows):
    def normalize_xyz(xyz):
        black = [0.1901, 0.2, 0.2178] # sRGB, reference black point, absolute XYZ
        white = [76.04, 80.0, 87.12] # sRGB, reference white point, absolute XYZ
        return [
            (xyz[0] - black[0]) / (white[0] - black[0]) * (white[0] / white[1]),
            (xyz[1] - black[1]) / (white[1] - black[1]),
            (xyz[2] - black[2]) / (white[2] - black[2]) * (white[2] / white[1]),
        ]

    # Converts normalized XYZ tristimulus values and converts to normalized RGB tristimulus values
    def xyz_to_rgb(xyz):
        # Matrix from https://en.wikipedia.org/wiki/SRGB
        rgb = [ 
             3.2406255 * xyz[0] - 1.5372080 * xyz[1] - 0.4986286 * xyz[2],
            -0.9689307 * xyz[0] + 1.8757561 * xyz[1] + 0.0415175 * xyz[2],
             0.0557101 * xyz[0] - 0.2040211 * xyz[1] + 1.0569959 * xyz[2],
        ]

        return [min(1, max(s, 0)) for s in rgb]

    def linear_rgb_to_srgb(rgb):
        def quantize(s):
            if s <= 0.0031308:
                return 12.92 * s
            return 1.055 * (s ** (1 / 2.4)) - 0.055

        return [quantize(s) for s in rgb]

    line = ''

    for [nm, *xyz] in rows:
        xyz = list(map(float, xyz))
        xyz = normalize_xyz([70 * s for s in xyz])
        rgb = xyz_to_rgb(xyz)
        rgb = linear_rgb_to_srgb(rgb)
        line += f'{int(rgb[0] * 255)} {int(rgb[1] * 255)} {int(rgb[2] * 255)} '
    
    width = 471
    height = 48
    print(f'P3 {width} {height} 255')
    print('\n'.join(height * [line]))

if __name__ == '__main__':
    f = open('CIE_xyz_1931_2deg.csv')
    reader = csv.reader(f)
    rows = list(reader)
    cols = list(zip(*rows))

    print_ppm(rows)
    #print_c_lut(cols)
