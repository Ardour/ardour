/**
 * @file colorspace.c
 * @author Pascal Getreuer 2005-2010 <getreuer@gmail.com>
 *
 * == Summary ==
 * This file implements routines for color transformations between the spaces
 * sRGB, Y'UV, Y'CbCr, Y'PbPr, Y'DbDr, Y'IQ, HSV, HSL, HSI, CIEXYZ, CIELAB,
 * CIELUV, CIELCH, and CIECAT02 LMS.
 *
 * == Usage ==
 * First call GetColorTransform, specifying the source and destination color
 * spaces as "dest<-src" or "src->dest".  Then call ApplyColorTransform to
 * perform the transform:
@code
       double S[3] = {173, 0.8, 0.5};
       double D[3];
       colortransform Trans;

       if(!(GetColorTransform(&Trans, "HSI -> Lab")))
       {
           printf("Invalid syntax or unknown color space\n");
           return;
       }

       ApplyColorTransform(Trans, &D[0], &D[1], &D[2], S[0], S[1], S[2]);
@endcode
 * "num" is a typedef defined at the beginning of colorspace.h that may be set
 * to either double or float, depending on the application.
 *
 * Specific transformation routines can also be called directly.  The following
 * converts an sRGB color to CIELAB and then back to sRGB:
@code
     double R = 0.85, G = 0.32, B = 0.5;
     double L, a, b;
     Rgb2Lab(&L, &a, &b, R, G, B);
     Lab2Rgb(&R, &G, &B, L, a, b);
@endcode
 * Generally, the calling syntax is
@code
     Foo2Bar(&B0, &B1, &B2, F0, F1, F2);
@endcode
 * where (F0,F1,F2) are the coordinates of a color in space "Foo" and
 * (B0,B1,B2) are the transformed coordinates in space "Bar."  For any
 * transformation routine, its inverse has the sytax
@code
     Bar2Foo(&F0, &F1, &F2, B0, B1, B2);
@endcode
 *
 * The conversion routines are consistently named with the first letter of a
 * color space capitalized with following letters in lower case and omitting
 * prime symbols.  For example, "Rgb2Ydbdr" converts sRGB to Y'DbDr.  For
 * any transformation routine Foo2Bar, its inverse is Bar2Foo.
 *
 * All transformations assume a two degree observer angle and a D65 illuminant.
 * The white point can be changed by modifying the WHITEPOINT_X, WHITEPOINT_Y,
 * WHITEPOINT_Z definitions at the beginning of colorspace.h.
 *
 * == List of transformation routines ==
 *   - Rgb2Yuv(double *Y, double *U, double *V, double R, double G, double B)
 *   - Rgb2Ycbcr(double *Y, double *Cb, double *Cr, double R, double G, double B)
 *   - Rgb2Jpegycbcr(double *Y, double *Cb, double *Cr, double R, double G, double B)
 *   - Rgb2Ypbpr(double *Y, double *Pb, double *Pr, double R, double G, double B)
 *   - Rgb2Ydbdr(double *Y, double *Db, double *Dr, double R, double G, double B)
 *   - Rgb2Yiq(double *Y, double *I, double *Q, double R, double G, double B)
 *   - Rgb2Hsv(double *H, double *S, double *V, double R, double G, double B)
 *   - Rgb2Hsl(double *H, double *S, double *L, double R, double G, double B)
 *   - Rgb2Hsi(double *H, double *S, double *I, double R, double G, double B)
 *   - Rgb2Xyz(double *X, double *Y, double *Z, double R, double G, double B)
 *   - Xyz2Lab(double *L, double *a, double *b, double X, double Y, double Z)
 *   - Xyz2Luv(double *L, double *u, double *v, double X, double Y, double Z)
 *   - Xyz2Lch(double *L, double *C, double *h, double X, double Y, double Z)
 *   - Xyz2Cat02lms(double *L, double *M, double *S, double X, double Y, double Z)
 *   - Rgb2Lab(double *L, double *a, double *b, double R, double G, double B)
 *   - Rgb2Luv(double *L, double *u, double *v, double R, double G, double B)
 *   - Rgb2Lch(double *L, double *C, double *h, double R, double G, double B)
 *   - Rgb2Cat02lms(double *L, double *M, double *S, double R, double G, double B)
 * (Similarly for the inverse transformations.)
 *
 * It is possible to transform between two arbitrary color spaces by first
 * transforming from the source space to sRGB and then transforming from
 * sRGB to the desired destination space.  For transformations between CIE
 * color spaces, it is convenient to use XYZ as the intermediate space.  This
 * is the strategy used by GetColorTransform and ApplyColorTransform.
 *
 * == References ==
 * The definitions of these spaces and the many of the transformation formulas
 * can be found in
 *
 *    Poynton, "Frequently Asked Questions About Gamma"
 *    http://www.poynton.com/notes/colour_and_gamma/GammaFAQ.html
 *
 *    Poynton, "Frequently Asked Questions About Color"
 *    http://www.poynton.com/notes/colour_and_gamma/ColorFAQ.html
 *
 * and Wikipedia articles
 *    http://en.wikipedia.org/wiki/SRGB
 *    http://en.wikipedia.org/wiki/YUV
 *    http://en.wikipedia.org/wiki/YCbCr
 *    http://en.wikipedia.org/wiki/YPbPr
 *    http://en.wikipedia.org/wiki/YDbDr
 *    http://en.wikipedia.org/wiki/YIQ
 *    http://en.wikipedia.org/wiki/HSL_and_HSV
 *    http://en.wikipedia.org/wiki/CIE_1931_color_space
 *    http://en.wikipedia.org/wiki/Lab_color_space
 *    http://en.wikipedia.org/wiki/CIELUV_color_space
 *    http://en.wikipedia.org/wiki/LMS_color_space
 *
 * == License (BSD) ==
 * Copyright (c) 2005-2010, Pascal Getreuer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gtkmm2ext/colorspace.h"

namespace Gtkmm2ext {

/** @brief Min of A and B */
#define MIN(A,B)	(((A) <= (B)) ? (A) : (B))

/** @brief Max of A and B */
#define MAX(A,B)	(((A) >= (B)) ? (A) : (B))

/** @brief Min of A, B, and C */
#define MIN3(A,B,C)	(((A) <= (B)) ? MIN(A,C) : MIN(B,C))

/** @brief Max of A, B, and C */
#define MAX3(A,B,C)	(((A) >= (B)) ? MAX(A,C) : MAX(B,C))

#ifndef M_PI
/** @brief The constant pi */
#define M_PI	3.14159265358979323846264338327950288
#endif

/**
 * @brief sRGB gamma correction, transforms R to R'
 * http://en.wikipedia.org/wiki/SRGB
 */
#define GAMMACORRECTION(t)	\
	(((t) <= 0.0031306684425005883) ? \
	(12.92*(t)) : (1.055*pow((t), 0.416666666666666667) - 0.055))

/**
 * @brief Inverse sRGB gamma correction, transforms R' to R
 */
#define INVGAMMACORRECTION(t)	\
	(((t) <= 0.0404482362771076) ? \
	((t)/12.92) : pow(((t) + 0.055)/1.055, 2.4))

/**
 * @brief CIE L*a*b* f function (used to convert XYZ to L*a*b*)
 * http://en.wikipedia.org/wiki/Lab_color_space
 */
#define LABF(t)	\
	((t >= 8.85645167903563082e-3) ? \
	pow(t,0.333333333333333) : (841.0/108.0)*(t) + (4.0/29.0))

/**
 * @brief CIE L*a*b* inverse f function
 * http://en.wikipedia.org/wiki/Lab_color_space
 */
#define LABINVF(t)	\
	((t >= 0.206896551724137931) ? \
	((t)*(t)*(t)) : (108.0/841.0)*((t) - (4.0/29.0)))

/** @brief u'v' coordinates of the white point for CIE Lu*v* */
#define WHITEPOINT_U	((4*WHITEPOINT_X) \
	/(WHITEPOINT_X + 15*WHITEPOINT_Y + 3*WHITEPOINT_Z))
#define WHITEPOINT_V	((9*WHITEPOINT_Y) \
	/(WHITEPOINT_X + 15*WHITEPOINT_Y + 3*WHITEPOINT_Z))

/** @brief Enumeration of the supported color spaces */
#define UNKNOWN_SPACE	0
#define RGB_SPACE		1
#define YUV_SPACE		2
#define YCBCR_SPACE		3
#define JPEGYCBCR_SPACE	4
#define YPBPR_SPACE		5
#define YDBDR_SPACE		6
#define YIQ_SPACE		7
#define HSV_SPACE		8
#define HSL_SPACE		9
#define HSI_SPACE		10
#define XYZ_SPACE		11
#define LAB_SPACE		12
#define LUV_SPACE		13
#define LCH_SPACE		14
#define CAT02LMS_SPACE	15

#define NUM_TRANSFORM_PAIRS		18



/*
 * == Linear color transformations ==
 *
 * The following routines implement transformations between sRGB and
 * the linearly-related color spaces Y'UV, Y'PbPr, Y'DbDr, and Y'IQ.
 */


/**
 * @brief Convert sRGB to NTSC/PAL Y'UV Luma + Chroma
 *
 * @param Y, U, V pointers to hold the result
 * @param R, G, B the input sRGB values
 *
 * Wikipedia: http://en.wikipedia.org/wiki/YUV
 */
void Rgb2Yuv(double *Y, double *U, double *V, double R, double G, double B)
{
	*Y = (double)( 0.299*R + 0.587*G + 0.114*B);
	*U = (double)(-0.147*R - 0.289*G + 0.436*B);
	*V = (double)( 0.615*R - 0.515*G - 0.100*B);
}


/**
 * @brief Convert NTSC/PAL Y'UV to sRGB
 *
 * @param R, G, B pointers to hold the result
 * @param Y, U, V the input YUV values
 */
void Yuv2Rgb(double *R, double *G, double *B, double Y, double U, double V)
{
	*R = (double)(Y - 3.945707070708279e-05*U + 1.1398279671717170825*V);
	*G = (double)(Y - 0.3946101641414141437*U - 0.5805003156565656797*V);
	*B = (double)(Y + 2.0319996843434342537*U - 4.813762626262513e-04*V);
}


/** @brief sRGB to Y'CbCr Luma + Chroma */
void Rgb2Ycbcr(double *Y, double *Cb, double *Cr, double R, double G, double B)
{
	*Y  = (double)( 65.481*R + 128.553*G +  24.966*B +  16);
	*Cb = (double)(-37.797*R -  74.203*G + 112.0  *B + 128);
	*Cr = (double)(112.0  *R -  93.786*G -  18.214*B + 128);
}


/** @brief Y'CbCr to sRGB */
void Ycbcr2Rgb(double *R, double *G, double *B, double Y, double Cr, double Cb)
{
	Y -= 16;
	Cb -= 128;
	Cr -= 128;
	*R = (double)(0.00456621004566210107*Y + 1.1808799897946415e-09*Cr + 0.00625892896994393634*Cb);
	*G = (double)(0.00456621004566210107*Y - 0.00153632368604490212*Cr - 0.00318811094965570701*Cb);
	*B = (double)(0.00456621004566210107*Y + 0.00791071623355474145*Cr + 1.1977497040190077e-08*Cb);
}


/** @brief sRGB to JPEG-Y'CbCr Luma + Chroma */
void Rgb2Jpegycbcr(double *Y, double *Cb, double *Cr, double R, double G, double B)
{
	Rgb2Ypbpr(Y, Cb, Cr, R, G, B);
	*Cb += (double)0.5;
	*Cr += (double)0.5;
}

/** @brief JPEG-Y'CbCr to sRGB */
void Jpegycbcr2Rgb(double *R, double *G, double *B, double Y, double Cb, double Cr)
{
	Cb -= (double)0.5;
	Cr -= (double)0.5;
	Ypbpr2Rgb(R, G, B, Y, Cb, Cr);
}


/** @brief sRGB to Y'PbPr Luma (ITU-R BT.601) + Chroma */
void Rgb2Ypbpr(double *Y, double *Pb, double *Pr, double R, double G, double B)
{
	*Y  = (double)( 0.299    *R + 0.587   *G + 0.114   *B);
	*Pb = (double)(-0.1687367*R - 0.331264*G + 0.5     *B);
	*Pr = (double)( 0.5      *R - 0.418688*G - 0.081312*B);
}


/** @brief Y'PbPr to sRGB */
void Ypbpr2Rgb(double *R, double *G, double *B, double Y, double Pb, double Pr)
{
	*R = (double)(0.99999999999914679361*Y - 1.2188941887145875e-06*Pb + 1.4019995886561440468*Pr);
	*G = (double)(0.99999975910502514331*Y - 0.34413567816504303521*Pb - 0.71413649331646789076*Pr);
	*B = (double)(1.00000124040004623180*Y + 1.77200006607230409200*Pb + 2.1453384174593273e-06*Pr);
}


/** @brief sRGB to SECAM Y'DbDr Luma + Chroma */
void Rgb2Ydbdr(double *Y, double *Db, double *Dr, double R, double G, double B)
{
	*Y  = (double)( 0.299*R + 0.587*G + 0.114*B);
	*Db = (double)(-0.450*R - 0.883*G + 1.333*B);
	*Dr = (double)(-1.333*R + 1.116*G + 0.217*B);
}


/** @brief SECAM Y'DbDr to sRGB */
void Ydbdr2Rgb(double *R, double *G, double *B, double Y, double Db, double Dr)
{
	*R = (double)(Y + 9.2303716147657e-05*Db - 0.52591263066186533*Dr);
	*G = (double)(Y - 0.12913289889050927*Db + 0.26789932820759876*Dr);
	*B = (double)(Y + 0.66467905997895482*Db - 7.9202543533108e-05*Dr);
}


/** @brief sRGB to NTSC YIQ */
void Rgb2Yiq(double *Y, double *I, double *Q, double R, double G, double B)
{
	*Y = (double)(0.299   *R + 0.587   *G + 0.114   *B);
	*I = (double)(0.595716*R - 0.274453*G - 0.321263*B);
	*Q = (double)(0.211456*R - 0.522591*G + 0.311135*B);
}


/** @brief Convert NTSC YIQ to sRGB */
void Yiq2Rgb(double *R, double *G, double *B, double Y, double I, double Q)
{
	*R = (double)(Y + 0.9562957197589482261*I + 0.6210244164652610754*Q);
	*G = (double)(Y - 0.2721220993185104464*I - 0.6473805968256950427*Q);
	*B = (double)(Y - 1.1069890167364901945*I + 1.7046149983646481374*Q);
}



/*
 * == Hue Saturation Value/Lightness/Intensity color transformations ==
 *
 * The following routines implement transformations between sRGB and
 * color spaces HSV, HSL, and HSI.
 */


/**
 * @brief Convert an sRGB color to Hue-Saturation-Value (HSV)
 *
 * @param H, S, V pointers to hold the result
 * @param R, G, B the input sRGB values scaled in [0,1]
 *
 * This routine transforms from sRGB to the hexcone HSV color space.  The
 * sRGB values are assumed to be between 0 and 1.  The output values are
 *   H = hexagonal hue angle   (0 <= H < 360),
 *   S = C/V                   (0 <= S <= 1),
 *   V = max(R',G',B')         (0 <= V <= 1),
 * where C = max(R',G',B') - min(R',G',B').  The inverse color transformation
 * is given by Hsv2Rgb.
 *
 * Wikipedia: http://en.wikipedia.org/wiki/HSL_and_HSV
 */
void Rgb2Hsv(double *H, double *S, double *V, double R, double G, double B)
{
	double Max = MAX3(R, G, B);
	double Min = MIN3(R, G, B);
	double C = Max - Min;


	*V = Max;

	if(C > 0)
	{
		if(Max == R)
		{
			*H = (G - B) / C;

			if(G < B)
				*H += 6;
		}
		else if(Max == G)
			*H = 2 + (B - R) / C;
		else
			*H = 4 + (R - G) / C;

		*H *= 60;
		*S = C / Max;
	}
	else
		*H = *S = 0;
}


/**
 * @brief Convert a Hue-Saturation-Value (HSV) color to sRGB
 *
 * @param R, G, B pointers to hold the result
 * @param H, S, V the input HSV values
 *
 * The input values are assumed to be scaled as
 *    0 <= H < 360,
 *    0 <= S <= 1,
 *    0 <= V <= 1.
 * The output sRGB values are scaled between 0 and 1.  This is the inverse
 * transformation of Rgb2Hsv.
 *
 * Wikipedia: http://en.wikipedia.org/wiki/HSL_and_HSV
 */
void Hsv2Rgb(double *R, double *G, double *B, double H, double S, double V)
{
	double C = S * V;
	double Min = V - C;
	double X;


	H -= 360*floor(H/360);
	H /= 60;
	X = C*(1 - fabs(H - 2*floor(H/2) - 1));

	switch((int)H)
	{
	case 0:
		*R = Min + C;
		*G = Min + X;
		*B = Min;
		break;
	case 1:
		*R = Min + X;
		*G = Min + C;
		*B = Min;
		break;
	case 2:
		*R = Min;
		*G = Min + C;
		*B = Min + X;
		break;
	case 3:
		*R = Min;
		*G = Min + X;
		*B = Min + C;
		break;
	case 4:
		*R = Min + X;
		*G = Min;
		*B = Min + C;
		break;
	case 5:
		*R = Min + C;
		*G = Min;
		*B = Min + X;
		break;
	default:
		*R = *G = *B = 0;
	}
}


/**
 * @brief Convert an sRGB color to Hue-Saturation-Lightness (HSL)
 *
 * @param H, S, L pointers to hold the result
 * @param R, G, B the input sRGB values scaled in [0,1]
 *
 * This routine transforms from sRGB to the double hexcone HSL color space
 * The sRGB values are assumed to be between 0 and 1.  The outputs are
 *   H = hexagonal hue angle                (0 <= H < 360),
 *   S = { C/(2L)     if L <= 1/2           (0 <= S <= 1),
 *       { C/(2 - 2L) if L >  1/2
 *   L = (max(R',G',B') + min(R',G',B'))/2  (0 <= L <= 1),
 * where C = max(R',G',B') - min(R',G',B').  The inverse color transformation
 * is given by Hsl2Rgb.
 *
 * Wikipedia: http://en.wikipedia.org/wiki/HSL_and_HSV
 */
void Rgb2Hsl(double *H, double *S, double *L, double R, double G, double B)
{
	double Max = MAX3(R, G, B);
	double Min = MIN3(R, G, B);
	double C = Max - Min;


	*L = (Max + Min)/2;

	if(C > 0)
	{
		if(Max == R)
		{
			*H = (G - B) / C;

			if(G < B)
				*H += 6;
		}
		else if(Max == G)
			*H = 2 + (B - R) / C;
		else
			*H = 4 + (R - G) / C;

		*H *= 60;
		*S = (*L <= 0.5) ? (C/(2*(*L))) : (C/(2 - 2*(*L)));
	}
	else
		*H = *S = 0;
}


/**
 * @brief Convert a Hue-Saturation-Lightness (HSL) color to sRGB
 *
 * @param R, G, B pointers to hold the result
 * @param H, S, L the input HSL values
 *
 * The input values are assumed to be scaled as
 *    0 <= H < 360,
 *    0 <= S <= 1,
 *    0 <= L <= 1.
 * The output sRGB values are scaled between 0 and 1.  This is the inverse
 * transformation of Rgb2Hsl.
 *
 * Wikipedia: http://en.wikipedia.org/wiki/HSL_and_HSV
 */
void Hsl2Rgb(double *R, double *G, double *B, double H, double S, double L)
{
	double C = (L <= 0.5) ? (2*L*S) : ((2 - 2*L)*S);
	double Min = L - 0.5*C;
	double X;


	H -= 360*floor(H/360);
	H /= 60;
	X = C*(1 - fabs(H - 2*floor(H/2) - 1));

	switch((int)H)
	{
	case 0:
		*R = Min + C;
		*G = Min + X;
		*B = Min;
		break;
	case 1:
		*R = Min + X;
		*G = Min + C;
		*B = Min;
		break;
	case 2:
		*R = Min;
		*G = Min + C;
		*B = Min + X;
		break;
	case 3:
		*R = Min;
		*G = Min + X;
		*B = Min + C;
		break;
	case 4:
		*R = Min + X;
		*G = Min;
		*B = Min + C;
		break;
	case 5:
		*R = Min + C;
		*G = Min;
		*B = Min + X;
		break;
	default:
		*R = *G = *B = 0;
	}
}


/**
 * @brief Convert an sRGB color to Hue-Saturation-Intensity (HSI)
 *
 * @param H, S, I pointers to hold the result
 * @param R, G, B the input sRGB values scaled in [0,1]
 *
 * This routine transforms from sRGB to the cylindrical HSI color space.  The
 * sRGB values are assumed to be between 0 and 1.  The output values are
 *   H = polar hue angle         (0 <= H < 360),
 *   S = 1 - min(R',G',B')/I     (0 <= S <= 1),
 *   I = (R'+G'+B')/3            (0 <= I <= 1).
 * The inverse color transformation is given by Hsi2Rgb.
 *
 * Wikipedia: http://en.wikipedia.org/wiki/HSL_and_HSV
 */
void Rgb2Hsi(double *H, double *S, double *I, double R, double G, double B)
{
	double alpha = 0.5*(2*R - G - B);
	double beta = 0.866025403784439*(G - B);


	*I = (R + G + B)/3;

	if(*I > 0)
	{
		*S = 1 - MIN3(R,G,B) / *I;
		*H = atan2(beta, alpha)*(180/M_PI);

		if(*H < 0)
			*H += 360;
	}
	else
		*H = *S = 0;
}


/**
 * @brief Convert a Hue-Saturation-Intesity (HSI) color to sRGB
 *
 * @param R, G, B pointers to hold the result
 * @param H, S, I the input HSI values
 *
 * The input values are assumed to be scaled as
 *    0 <= H < 360,
 *    0 <= S <= 1,
 *    0 <= I <= 1.
 * The output sRGB values are scaled between 0 and 1.  This is the inverse
 * transformation of Rgb2Hsi.
 *
 * Wikipedia: http://en.wikipedia.org/wiki/HSL_and_HSV
 */
void Hsi2Rgb(double *R, double *G, double *B, double H, double S, double I)
{
	H -= 360*floor(H/360);

	if(H < 120)
	{
		*B = I*(1 - S);
		*R = I*(1 + S*cos(H*(M_PI/180))/cos((60 - H)*(M_PI/180)));
		*G = 3*I - *R - *B;
	}
	else if(H < 240)
	{
		H -= 120;
		*R = I*(1 - S);
		*G = I*(1 + S*cos(H*(M_PI/180))/cos((60 - H)*(M_PI/180)));
		*B = 3*I - *R - *G;
	}
	else
	{
		H -= 240;
		*G = I*(1 - S);
		*B = I*(1 + S*cos(H*(M_PI/180))/cos((60 - H)*(M_PI/180)));
		*R = 3*I - *G - *B;
	}
}


/*
 * == CIE color transformations ==
 *
 * The following routines implement transformations between sRGB and
 * the CIE color spaces XYZ, L*a*b, L*u*v*, and L*C*H*.  These
 * transforms assume a 2 degree observer angle and a D65 illuminant.
 */


/**
 * @brief Transform sRGB to CIE XYZ with the D65 white point
 *
 * @param X, Y, Z pointers to hold the result
 * @param R, G, B the input sRGB values
 *
 * Poynton, "Frequently Asked Questions About Color," page 10
 * Wikipedia: http://en.wikipedia.org/wiki/SRGB
 * Wikipedia: http://en.wikipedia.org/wiki/CIE_1931_color_space
 */
void Rgb2Xyz(double *X, double *Y, double *Z, double R, double G, double B)
{
	R = INVGAMMACORRECTION(R);
	G = INVGAMMACORRECTION(G);
	B = INVGAMMACORRECTION(B);
	*X = (double)(0.4123955889674142161*R + 0.3575834307637148171*G + 0.1804926473817015735*B);
	*Y = (double)(0.2125862307855955516*R + 0.7151703037034108499*G + 0.07220049864333622685*B);
	*Z = (double)(0.01929721549174694484*R + 0.1191838645808485318*G + 0.9504971251315797660*B);
}


/**
 * @brief Transform CIE XYZ to sRGB with the D65 white point
 *
 * @param R, G, B pointers to hold the result
 * @param X, Y, Z the input XYZ values
 *
 * Official sRGB specification (IEC 61966-2-1:1999)
 * Poynton, "Frequently Asked Questions About Color," page 10
 * Wikipedia: http://en.wikipedia.org/wiki/SRGB
 * Wikipedia: http://en.wikipedia.org/wiki/CIE_1931_color_space
 */
void Xyz2Rgb(double *R, double *G, double *B, double X, double Y, double Z)
{
	double R1, B1, G1, Min;


	R1 = (double)( 3.2406*X - 1.5372*Y - 0.4986*Z);
	G1 = (double)(-0.9689*X + 1.8758*Y + 0.0415*Z);
	B1 = (double)( 0.0557*X - 0.2040*Y + 1.0570*Z);

	Min = MIN3(R1, G1, B1);

	/* Force nonnegative values so that gamma correction is well-defined. */
	if(Min < 0)
	{
		R1 -= Min;
		G1 -= Min;
		B1 -= Min;
	}

	/* Transform from RGB to R'G'B' */
	*R = GAMMACORRECTION(R1);
	*G = GAMMACORRECTION(G1);
	*B = GAMMACORRECTION(B1);
}


/**
 * Convert CIE XYZ to CIE L*a*b* (CIELAB) with the D65 white point
 *
 * @param L, a, b pointers to hold the result
 * @param X, Y, Z the input XYZ values
 *
 * Wikipedia: http://en.wikipedia.org/wiki/Lab_color_space
 */
void Xyz2Lab(double *L, double *a, double *b, double X, double Y, double Z)
{
	X /= WHITEPOINT_X;
	Y /= WHITEPOINT_Y;
	Z /= WHITEPOINT_Z;
	X = LABF(X);
	Y = LABF(Y);
	Z = LABF(Z);
	*L = 116*Y - 16;
	*a = 500*(X - Y);
	*b = 200*(Y - Z);
}


/**
 * Convert CIE L*a*b* (CIELAB) to CIE XYZ with the D65 white point
 *
 * @param X, Y, Z pointers to hold the result
 * @param L, a, b the input L*a*b* values
 *
 * Wikipedia: http://en.wikipedia.org/wiki/Lab_color_space
 */
void Lab2Xyz(double *X, double *Y, double *Z, double L, double a, double b)
{
	L = (L + 16)/116;
	a = L + a/500;
	b = L - b/200;
	*X = WHITEPOINT_X*LABINVF(a);
	*Y = WHITEPOINT_Y*LABINVF(L);
	*Z = WHITEPOINT_Z*LABINVF(b);
}


/**
 * Convert CIE XYZ to CIE L*u*v* (CIELUV) with the D65 white point
 *
 * @param L, u, v pointers to hold the result
 * @param X, Y, Z the input XYZ values
 *
 * Wikipedia: http://en.wikipedia.org/wiki/CIELUV_color_space
 */
void Xyz2Luv(double *L, double *u, double *v, double X, double Y, double Z)
{
	double u1, v1, Denom;


	if((Denom = X + 15*Y + 3*Z) > 0)
	{
		u1 = (4*X) / Denom;
		v1 = (9*Y) / Denom;
	}
	else
		u1 = v1 = 0;

	Y /= WHITEPOINT_Y;
	Y = LABF(Y);
	*L = 116*Y - 16;
	*u = 13*(*L)*(u1 - WHITEPOINT_U);
	*v = 13*(*L)*(v1 - WHITEPOINT_V);
}


/**
 * Convert CIE L*u*v* (CIELUV) to CIE XYZ with the D65 white point
 *
 * @param X, Y, Z pointers to hold the result
 * @param L, u, v the input L*u*v* values
 *
 * Wikipedia: http://en.wikipedia.org/wiki/CIELUV_color_space
 */
void Luv2Xyz(double *X, double *Y, double *Z, double L, double u, double v)
{
	*Y = (L + 16)/116;
	*Y = WHITEPOINT_Y*LABINVF(*Y);

	if(L != 0)
	{
		u /= L;
		v /= L;
	}

	u = u/13 + WHITEPOINT_U;
	v = v/13 + WHITEPOINT_V;
	*X = (*Y) * ((9*u)/(4*v));
	*Z = (*Y) * ((3 - 0.75*u)/v - 5);
}


/**
 * Convert CIE XYZ to CIE L*C*H* with the D65 white point
 *
 * @param L, C, H pointers to hold the result
 * @param X, Y, Z the input XYZ values
 *
 * CIE L*C*H* is related to CIE L*a*b* by
 *    a* = C* cos(H* pi/180),
 *    b* = C* sin(H* pi/180).
 */
void Xyz2Lch(double *L, double *C, double *H, double X, double Y, double Z)
{
	double a, b;


	Xyz2Lab(L, &a, &b, X, Y, Z);
	*C = sqrt(a*a + b*b);
	*H = atan2(b, a)*180.0/M_PI;

	if(*H < 0)
		*H += 360;
}

/**
 * Convert CIE L*C*H* to CIE XYZ with the D65 white point
 *
 * @param X, Y, Z pointers to hold the result
 * @param L, C, H the input L*C*H* values
 */
void Lch2Xyz(double *X, double *Y, double *Z, double L, double C, double H)
{
	double a = C * cos(H*(M_PI/180.0));
	double b = C * sin(H*(M_PI/180.0));


	Lab2Xyz(X, Y, Z, L, a, b);
}


/** @brief XYZ to CAT02 LMS */
void Xyz2Cat02lms(double *L, double *M, double *S, double X, double Y, double Z)
{
	*L = (double)( 0.7328*X + 0.4296*Y - 0.1624*Z);
	*M = (double)(-0.7036*X + 1.6975*Y + 0.0061*Z);
	*S = (double)( 0.0030*X + 0.0136*Y + 0.9834*Z);
}


/** @brief CAT02 LMS to XYZ */
void Cat02lms2Xyz(double *X, double *Y, double *Z, double L, double M, double S)
{
	*X = (double)( 1.096123820835514*L - 0.278869000218287*M + 0.182745179382773*S);
	*Y = (double)( 0.454369041975359*L + 0.473533154307412*M + 0.072097803717229*S);
	*Z = (double)(-0.009627608738429*L - 0.005698031216113*M + 1.015325639954543*S);
}


/*
 * == Glue functions for multi-stage transforms ==
 */

void Rgb2Lab(double *L, double *a, double *b, double R, double G, double B)
{
	double X, Y, Z;
	Rgb2Xyz(&X, &Y, &Z, R, G, B);
	Xyz2Lab(L, a, b, X, Y, Z);
}


void Lab2Rgb(double *R, double *G, double *B, double L, double a, double b)
{
	double X, Y, Z;
	Lab2Xyz(&X, &Y, &Z, L, a, b);
	Xyz2Rgb(R, G, B, X, Y, Z);
}


void Rgb2Luv(double *L, double *u, double *v, double R, double G, double B)
{
	double X, Y, Z;
	Rgb2Xyz(&X, &Y, &Z, R, G, B);
	Xyz2Luv(L, u, v, X, Y, Z);
}


void Luv2Rgb(double *R, double *G, double *B, double L, double u, double v)
{
	double X, Y, Z;
	Luv2Xyz(&X, &Y, &Z, L, u, v);
	Xyz2Rgb(R, G, B, X, Y, Z);
}

void Rgb2Lch(double *L, double *C, double *H, double R, double G, double B)
{
	double X, Y, Z;
	Rgb2Xyz(&X, &Y, &Z, R, G, B);
	Xyz2Lch(L, C, H, X, Y, Z);
}


void Lch2Rgb(double *R, double *G, double *B, double L, double C, double H)
{
	double X, Y, Z;
	Lch2Xyz(&X, &Y, &Z, L, C, H);
	Xyz2Rgb(R, G, B, X, Y, Z);
}


void Rgb2Cat02lms(double *L, double *M, double *S, double R, double G, double B)
{
	double X, Y, Z;
	Rgb2Xyz(&X, &Y, &Z, R, G, B);
	Xyz2Cat02lms(L, M, S, X, Y, Z);
}


void Cat02lms2Rgb(double *R, double *G, double *B, double L, double M, double S)
{
	double X, Y, Z;
	Cat02lms2Xyz(&X, &Y, &Z, L, M, S);
	Xyz2Rgb(R, G, B, X, Y, Z);
}

} /* namespace */
