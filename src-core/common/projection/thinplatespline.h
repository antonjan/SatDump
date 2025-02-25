#pragma once

/******************************************************************************
 *
 *
 * Project:  GDAL Warp API
 * Purpose:  Declarations for 2D Thin Plate Spline transformer.
 * Author:   VIZRT Development Team.
 *
 * This code was provided by Gilad Ronnen (gro at visrt dot com) with
 * permission to reuse under the following license.
 *
 ******************************************************************************
 * Copyright (c) 2004, VIZRT Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

/*
 This file is originally from https://github.com/OSGeo/gdal
 It was modified for the purposes required here by Aang23.

 All credits go to the GDAL Project and the VIZRT Team.
*/

namespace satdump
{
    namespace projection
    {
        // Ground Control Point
        struct GCP
        {
            double x;
            double y;
            double lon;
            double lat;
        };

        typedef enum
        {
            VIZ_GEOREF_SPLINE_ZERO_POINTS,
            VIZ_GEOREF_SPLINE_ONE_POINT,
            VIZ_GEOREF_SPLINE_TWO_POINTS,
            VIZ_GEOREF_SPLINE_ONE_DIMENSIONAL,
            VIZ_GEOREF_SPLINE_FULL,

            VIZ_GEOREF_SPLINE_POINT_WAS_ADDED,
            VIZ_GEOREF_SPLINE_POINT_WAS_DELETED
        } vizGeorefInterType;

//#define VIZ_GEOREF_SPLINE_MAX_POINTS 40
#define VIZGEOREF_MAX_VARS 2

        class VizGeorefSpline2D
        {
            bool grow_points();

        public:
            explicit VizGeorefSpline2D(int nof_vars = 1)
                : type(VIZ_GEOREF_SPLINE_ZERO_POINTS),
                  _nof_vars(nof_vars),
                  _nof_points(0),
                  _max_nof_points(0),
                  _nof_eqs(0),
                  _dx(0.0),
                  _dy(0.0),
                  x(nullptr),
                  y(nullptr),
                  u(nullptr),
                  unused(nullptr),
                  index(nullptr),
                  x_mean(0),
                  y_mean(0)
            {
                for (int i = 0; i < VIZGEOREF_MAX_VARS; i++)
                {
                    rhs[i] = nullptr;
                    coef[i] = nullptr;
                }

                grow_points();
            }

            ~VizGeorefSpline2D()
            {
                delete x;
                delete y;
                delete u;
                delete unused;
                delete index;

                for (int i = 0; i < _nof_vars; i++)
                {
                    delete rhs[i];
                    delete coef[i];
                }
            }

            bool add_point(const double Px, const double Py, const double *Pvars);
            int get_point(const double Px, const double Py, double *Pvars);
            int solve(void);

        private:
            vizGeorefInterType type;

        public:
            const int _nof_vars;
            int _nof_points;
            int _max_nof_points;
            int _nof_eqs;

            double _dx, _dy;

            double *x; // [VIZ_GEOREF_SPLINE_MAX_POINTS+3];
            double *y; // [VIZ_GEOREF_SPLINE_MAX_POINTS+3];

            //    double rhs[VIZ_GEOREF_SPLINE_MAX_POINTS+3][VIZGEOREF_MAX_VARS];
            //    double coef[VIZ_GEOREF_SPLINE_MAX_POINTS+3][VIZGEOREF_MAX_VARS];
            double *rhs[VIZGEOREF_MAX_VARS];
            double *coef[VIZGEOREF_MAX_VARS];

            double *u;   // [VIZ_GEOREF_SPLINE_MAX_POINTS];
            int *unused; // [VIZ_GEOREF_SPLINE_MAX_POINTS];
            int *index;  // [VIZ_GEOREF_SPLINE_MAX_POINTS];

            double x_mean;
            double y_mean;
        };
    };
};