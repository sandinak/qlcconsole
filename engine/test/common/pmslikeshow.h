/*
  Q Light Controller Plus - test support
  pmslikeshow.h

  A trimmed show shaped like a school's house rig made from an Eos export
  (New Venue from a Show, docs/design/VENUE_FROM_SHOW.md): one universe
  sent by Art-Net on an office network, relays free on the plot on a hidden
  layer, front-of-house lights hung on a truss of that show's own, groups
  named as Eos named them. Made up; nothing in it is from a real file.

  Licensed under the Apache License, Version 2.0
*/

#ifndef QLC_TEST_PMSLIKESHOW_H
#define QLC_TEST_PMSLIKESHOW_H

#include <QByteArray>
#include <QString>

namespace PmsLikeShow
{
/* Relays: ids 1-4, 1 channel each at addresses 1-4 (zero-based 0-3), free
   on the plot at (3000 + 400 i, 760) mm on layer 2 (hidden).
   FOH: ids 51-53, 3 channels each at 101, 104, 107 (zero-based 100, 103,
   106), on truss 0 ("FOH": origin 4.96, 13.296, 6.002 m, along +X, 0.29 m
   wide) at 0.305, 1.219, 2.438 m, 0.391 m across, hung under it.
   Groups: "FOH" (51-53), "Relays" (1-4), "1st Elect." (1, 2).
   Its grid is 70 ft wide (its centre line at 10.668 m). With @p cyc,
   "Cyclorama 2" hangs across it 2 m from the grid's upstage edge (its
   upstage line, docs/design/VENUE_ROLES_DATUM.md). */
inline QByteArray xml(const QString &outputIp = QStringLiteral("172.18.2.10"), bool cyc = false)
{
    QByteArray out;
    out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE Workspace>\n"
           "<Workspace xmlns=\"http://www.qlcplus.org/Workspace\" CurrentWindow=\"Monitor\">\n"
           " <Creator><Name>qlcconsole</Name><Version>test</Version><Author>test</Author></Creator>\n"
           " <Engine>\n"
           "  <InputOutputMap>\n"
           "   <Universe Name=\"U01-Office\" ID=\"0\">\n"
           "    <Output Plugin=\"ArtNet\" UID=\"172.18.2.219\" Line=\"1\" Network=\"172.18.2.0/24\">\n"
           "     <PluginParameters outputIP=\"" + outputIp.toUtf8() + "\"/>\n"
           "    </Output>\n"
           "   </Universe>\n"
           "   <Universe Name=\"U02\" ID=\"1\"/>\n"
           "  </InputOutputMap>\n";
    for (int i = 0; i < 4; ++i)
        out += QStringLiteral("  <Fixture><Manufacturer>Generic</Manufacturer><Model>Generic</Model>"
                              "<Mode>1 Channel</Mode><ID>%1</ID><Name>Ch %1 Relay</Name><Universe>0</Universe>"
                              "<Address>%2</Address><Channels>1</Channels></Fixture>\n")
                   .arg(i + 1).arg(i).toUtf8();
    for (int i = 0; i < 3; ++i)
        out += QStringLiteral("  <Fixture><Manufacturer>Generic</Manufacturer><Model>Generic</Model>"
                              "<Mode>3 Channel</Mode><ID>%1</ID><Name>FOH %2</Name><Universe>0</Universe>"
                              "<Address>%3</Address><Channels>3</Channels></Fixture>\n")
                   .arg(51 + i).arg(i + 1).arg(100 + 3 * i).toUtf8();
    out += "  <FixtureGroup ID=\"0\"><Name>FOH</Name><Path>Eos</Path><Size X=\"1\" Y=\"3\"/>"
           "<Head X=\"0\" Y=\"0\" Fixture=\"51\">0</Head><Head X=\"0\" Y=\"1\" Fixture=\"52\">0</Head>"
           "<Head X=\"0\" Y=\"2\" Fixture=\"53\">0</Head></FixtureGroup>\n"
           "  <FixtureGroup ID=\"1\"><Name>Relays</Name><Path>Eos</Path><Size X=\"4\" Y=\"1\"/>"
           "<Head X=\"0\" Y=\"0\" Fixture=\"1\">0</Head><Head X=\"1\" Y=\"0\" Fixture=\"2\">0</Head>"
           "<Head X=\"2\" Y=\"0\" Fixture=\"3\">0</Head><Head X=\"3\" Y=\"0\" Fixture=\"4\">0</Head></FixtureGroup>\n"
           "  <FixtureGroup ID=\"2\"><Name>1st Elect.</Name><Path>Eos</Path><Size X=\"2\" Y=\"1\"/>"
           "<Head X=\"0\" Y=\"0\" Fixture=\"1\">0</Head><Head X=\"1\" Y=\"0\" Fixture=\"2\">0</Head></FixtureGroup>\n"
           "  <Monitor DisplayMode=\"1\" ShowLabels=\"1\">\n"
           "   <Grid Width=\"70\" Height=\"10\" Depth=\"40\" Units=\"1\"/>\n"
           "   <MonitorLayer ID=\"0\" Name=\"Default\" Visible=\"1\" Order=\"0\"/>\n"
           "   <MonitorLayer ID=\"2\" Name=\"Relays\" Visible=\"0\" Order=\"1\"/>\n"
           "   <MonitorLayer ID=\"3\" Name=\"Fly\" Visible=\"1\" Order=\"2\"/>\n";
    for (int i = 0; i < 4; ++i)
        out += QStringLiteral("   <FxItem ID=\"%1\" XPos=\"%2\" YPos=\"760\" LayerId=\"2\"/>\n")
                   .arg(i + 1).arg(3000 + 400 * i).toUtf8();
    const char *offsets[] = { "0.305", "1.219", "2.438" };
    out += "   <Truss ID=\"0\" Name=\"FOH\" Type=\"Horizontal\" Profile=\"Square\" Length=\"11.582\" "
           "Width=\"0.290\" OriginX=\"4.960\" OriginY=\"13.296\" OriginZ=\"6.002\" DirX=\"1.000000\" "
           "DirY=\"0.000000\" LayerId=\"3\"/>\n";
    for (int i = 0; i < 3; ++i)
        out += QStringLiteral("   <FixtureRig FID=\"%1\" Truss=\"0\" Offset=\"%2\" Mounting=\"TopHung\" "
                              "PanZero=\"212.1\" AimDown=\"43.0\" Lens=\"36.0\" TrussCross=\"0.391\" SM=\"0\"/>\n")
                   .arg(51 + i).arg(QLatin1String(offsets[i])).toUtf8();
    for (int i = 0; i < 3; ++i)
        out += QStringLiteral("   <FxItem ID=\"%1\" XPos=\"0\" YPos=\"0\" LayerId=\"3\"/>\n").arg(51 + i).toUtf8();
    if (cyc)
        out += "   <Scenery ID=\"0\" Name=\"Cyclorama 2\" Kind=\"0\" OriginX=\"10.668\" OriginY=\"2.000\" "
               "Width=\"18.000\" Height=\"6.000\"/>\n";
    out += "  </Monitor>\n </Engine>\n</Workspace>\n";
    return out;
}

/* The world position (mm across and up/down stage, metres high) FOH i
   (0-2) hangs at: on the truss, 0.391 m across, half the truss under it. */
inline double fohX(int i)
{
    const double offsets[] = { 0.305, 1.219, 2.438 };
    return (4.960 + offsets[i]) * 1000.0;
}
inline double fohY() { return (13.296 + 0.391) * 1000.0; }
inline double fohZ() { return 6.002 - 0.145; }
}

#endif
