/**
 * @file colorspace.h
 * @author Pascal Getreuer 2005-2010 <getreuer@gmail.com>
 */

#ifndef _GTKMM2EXT_COLORSPACE_H_
#define _GTKMM2EXT_COLORSPACE_H_

/** @brief XYZ color of the D65 white point */
#define WHITEPOINT_X	0.950456
#define WHITEPOINT_Y	1.0
#define WHITEPOINT_Z	1.088754

namespace Gtkmm2ext
{

void Rgb2Yuv(double *Y, double *U, double *V, double R, double G, double B);
void Yuv2Rgb(double *R, double *G, double *B, double Y, double U, double V);
void Rgb2Ycbcr(double *Y, double *Cb, double *Cr, double R, double G, double B);
void Ycbcr2Rgb(double *R, double *G, double *B, double Y, double Cb, double Cr);
void Rgb2Jpegycbcr(double *R, double *G, double *B, double Y, double Cb, double Cr);
void Jpegycbcr2Rgb(double *R, double *G, double *B, double Y, double Cb, double Cr);
void Rgb2Ypbpr(double *Y, double *Pb, double *Pr, double R, double G, double B);
void Ypbpr2Rgb(double *R, double *G, double *B, double Y, double Pb, double Pr);
void Rgb2Ydbdr(double *Y, double *Db, double *Dr, double R, double G, double B);
void Ydbdr2Rgb(double *R, double *G, double *B, double Y, double Db, double Dr);
void Rgb2Yiq(double *Y, double *I, double *Q, double R, double G, double B);
void Yiq2Rgb(double *R, double *G, double *B, double Y, double I, double Q);

void Rgb2Hsv(double *H, double *S, double *V, double R, double G, double B);
void Hsv2Rgb(double *R, double *G, double *B, double H, double S, double V);
void Rgb2Hsl(double *H, double *S, double *L, double R, double G, double B);
void Hsl2Rgb(double *R, double *G, double *B, double H, double S, double L);
void Rgb2Hsi(double *H, double *S, double *I, double R, double G, double B);
void Hsi2Rgb(double *R, double *G, double *B, double H, double S, double I);

void Rgb2Xyz(double *X, double *Y, double *Z, double R, double G, double B);
void Xyz2Rgb(double *R, double *G, double *B, double X, double Y, double Z);
void Xyz2Lab(double *L, double *a, double *b, double X, double Y, double Z);
void Lab2Xyz(double *X, double *Y, double *Z, double L, double a, double b);
void Xyz2Luv(double *L, double *u, double *v, double X, double Y, double Z);
void Luv2Xyz(double *X, double *Y, double *Z, double L, double u, double v);
void Xyz2Lch(double *L, double *C, double *H, double X, double Y, double Z);
void Lch2Xyz(double *X, double *Y, double *Z, double L, double C, double H);
void Xyz2Cat02lms(double *L, double *M, double *S, double X, double Y, double Z);
void Cat02lms2Xyz(double *X, double *Y, double *Z, double L, double M, double S);

void Rgb2Lab(double *L, double *a, double *b, double R, double G, double B);
void Lab2Rgb(double *R, double *G, double *B, double L, double a, double b);
void Rgb2Luv(double *L, double *u, double *v, double R, double G, double B);
void Luv2Rgb(double *R, double *G, double *B, double L, double u, double v);
void Rgb2Lch(double *L, double *C, double *H, double R, double G, double B);
void Lch2Rgb(double *R, double *G, double *B, double L, double C, double H);
void Rgb2Cat02lms(double *L, double *M, double *S, double R, double G, double B);
void Cat02lms2Rgb(double *R, double *G, double *B, double L, double M, double S);

} /* namespace */

#endif
