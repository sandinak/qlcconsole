/*
  Q Light Controller Plus
  humanfigure.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QtMath>

#include "humanfigure.h"

const QVector<FigureLoft> &humanFigureLofts()
{
    static const QVector<FigureLoft> lofts = []() {
        /* Measured off a 1.75 m adult in metres, then divided through, so a
           taller or shorter subject is the same body scaled. */
        const float H = 1.75f;
        auto ring = [H](float z, float cx, float cy, float rx, float ry) {
            return FigureRing{ z / H, cx / H, cy / H, rx / H, ry / H };
        };
        auto mirrored = [](const FigureLoft &l) {
            FigureLoft m = l;
            for (FigureRing &r : m)
                r.cx = -r.cx;
            return m;
        };

        QVector<FigureLoft> out;

        const FigureLoft leg = {
            ring(0.00f, 0.085f,  0.035f, 0.046f, 0.105f),   // sole
            ring(0.06f, 0.085f,  0.015f, 0.042f, 0.065f),   // top of the foot
            ring(0.11f, 0.085f,  0.000f, 0.034f, 0.038f),   // ankle
            ring(0.32f, 0.088f, -0.008f, 0.050f, 0.056f),   // calf
            ring(0.49f, 0.090f,  0.000f, 0.047f, 0.050f),   // knee
            ring(0.70f, 0.095f,  0.000f, 0.068f, 0.076f),   // thigh
            ring(0.87f, 0.090f,  0.000f, 0.080f, 0.086f) }; // hip
        out << leg << mirrored(leg);

        out << FigureLoft {
            ring(0.82f, 0.0f, 0.000f, 0.165f, 0.105f),      // seat
            ring(0.95f, 0.0f, 0.000f, 0.160f, 0.100f),      // hips
            ring(1.06f, 0.0f, 0.005f, 0.140f, 0.090f),      // waist
            ring(1.22f, 0.0f, 0.012f, 0.165f, 0.110f),      // chest
            ring(1.37f, 0.0f, 0.000f, 0.195f, 0.105f),      // shoulders
            ring(1.45f, 0.0f, 0.000f, 0.120f, 0.075f),      // top of the shoulders
            ring(1.47f, 0.0f, 0.000f, 0.055f, 0.055f) };    // base of the neck

        out << FigureLoft {
            ring(1.46f, 0.0f, 0.005f, 0.050f, 0.052f),
            ring(1.56f, 0.0f, 0.010f, 0.046f, 0.048f) };

        // The head, an ellipsoid sampled in latitude.
        FigureLoft head;
        const float hz = 1.645f, hh = 0.118f, hrx = 0.078f, hry = 0.095f;
        for (int deg = -84; deg <= 84; deg += 24)
        {
            const float c = float(qCos(qDegreesToRadians(double(deg))));
            const float s = float(qSin(qDegreesToRadians(double(deg))));
            head << ring(hz + hh * s, 0.0f, 0.012f, hrx * c, hry * c);
        }
        out << head;

        // Arms hang from the shoulders, a little away from the body.
        const FigureLoft arm = {
            ring(0.70f, 0.252f, 0.010f, 0.016f, 0.012f),    // fingertips
            ring(0.77f, 0.250f, 0.008f, 0.034f, 0.022f),    // hand
            ring(0.87f, 0.245f, 0.000f, 0.030f, 0.034f),    // wrist
            ring(1.12f, 0.235f, 0.000f, 0.040f, 0.045f),    // elbow
            ring(1.40f, 0.205f, 0.000f, 0.052f, 0.056f) };  // shoulder
        out << arm << mirrored(arm);

        return out;
    }();
    return lofts;
}
