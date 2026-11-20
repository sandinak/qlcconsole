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
   upstage line, docs/design/VENUE_ROLES_DATUM.md).
   With @p roles, the lights house roles are suggested for
   (VENUE_ROLES_DATUM.md section 4), each 1 channel unless said, free on
   the plot on layer 0:
     15 "Ch 15 Linear" at 15, 24 "Ch 24 Down" at 24: group "House" (both)
       and "House & Work" (with 17);
     17 "Ch 17 Stage Work Lights" at 17;
     22 "Ch 22 Hallway" at 22: a running light;
     101 "Ch 101" at 201, in group "2nd Elect", over the stage: 2 m from
       the grid's upstage edge, 6 m up;
     114 "Ch 114", Test "Focus Spot Three Z" (3ch: pan, tilt, dimmer) at 210;
     301 "Ch 301", Test "ColorSource CYC" (3ch RGB) at 301, in group
       "3rd Elect" (the test adds those two definitions).
   (Addresses 1-based here; the file's are zero-based.) */
inline QByteArray xml(const QString &outputIp = QStringLiteral("172.18.2.10"), bool cyc = false,
                      bool roles = false)
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
    if (roles)
    {
        const struct { int id; const char *name; int address; } dimmers[] = {
            { 15, "Ch 15 Linear", 15 }, { 24, "Ch 24 Down", 24 }, { 17, "Ch 17 Stage Work Lights", 17 },
            { 22, "Ch 22 Hallway", 22 }, { 101, "Ch 101", 201 } };
        for (const auto &d : dimmers)
            out += QStringLiteral("  <Fixture><Manufacturer>Generic</Manufacturer><Model>Generic</Model>"
                                  "<Mode>1 Channel</Mode><ID>%1</ID><Name>%2</Name><Universe>0</Universe>"
                                  "<Address>%3</Address><Channels>1</Channels></Fixture>\n")
                       .arg(d.id).arg(QLatin1String(d.name)).arg(d.address - 1).toUtf8();
        out += "  <Fixture><Manufacturer>Test</Manufacturer><Model>Focus Spot Three Z</Model><Mode>3ch</Mode>"
               "<ID>114</ID><Name>Ch 114</Name><Universe>0</Universe><Address>209</Address>"
               "<Channels>3</Channels></Fixture>\n"
               "  <Fixture><Manufacturer>Test</Manufacturer><Model>ColorSource CYC</Model><Mode>3ch RGB</Mode>"
               "<ID>301</ID><Name>Ch 301</Name><Universe>0</Universe><Address>300</Address>"
               "<Channels>3</Channels></Fixture>\n"
               "  <FixtureGroup ID=\"3\"><Name>House</Name><Path>Eos</Path><Size X=\"2\" Y=\"1\"/>"
               "<Head X=\"0\" Y=\"0\" Fixture=\"15\">0</Head><Head X=\"1\" Y=\"0\" Fixture=\"24\">0</Head></FixtureGroup>\n"
               "  <FixtureGroup ID=\"4\"><Name>House &amp; Work</Name><Path>Eos</Path><Size X=\"3\" Y=\"1\"/>"
               "<Head X=\"0\" Y=\"0\" Fixture=\"15\">0</Head><Head X=\"1\" Y=\"0\" Fixture=\"24\">0</Head>"
               "<Head X=\"2\" Y=\"0\" Fixture=\"17\">0</Head></FixtureGroup>\n"
               "  <FixtureGroup ID=\"5\"><Name>2nd Elect</Name><Path>Eos</Path><Size X=\"1\" Y=\"1\"/>"
               "<Head X=\"0\" Y=\"0\" Fixture=\"101\">0</Head></FixtureGroup>\n"
               "  <FixtureGroup ID=\"6\"><Name>3rd Elect</Name><Path>Eos</Path><Size X=\"1\" Y=\"1\"/>"
               "<Head X=\"0\" Y=\"0\" Fixture=\"301\">0</Head></FixtureGroup>\n";
    }
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
    if (roles)
    {
        const int placed[] = { 15, 24, 17, 22, 114, 301 };
        for (int i = 0; i < 6; ++i)
            out += QStringLiteral("   <FxItem ID=\"%1\" XPos=\"%2\" YPos=\"20000\" ZPos=\"3\" LayerId=\"0\"/>\n")
                       .arg(placed[i]).arg(4000 + 1000 * i).toUtf8();
        out += "   <FxItem ID=\"101\" XPos=\"10668\" YPos=\"2000\" ZPos=\"6\" LayerId=\"0\"/>\n";
    }
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
