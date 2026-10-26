/*
  Q Light Controller Plus
  xlsxwriter.cpp

  Copyright (C) Branson Matheson

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

#include <QDataStream>
#include <QIODevice>
#include <QDateTime>

#include "xlsxwriter.h"

quint32 XlsxWriter::crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool ready = false;
    if (!ready)
    {
        for (quint32 n = 0; n < 256; ++n)
        {
            quint32 c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (const char ch : data)
        crc = table[(crc ^ quint8(ch)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

QByteArray XlsxWriter::zip(const QList<QPair<QString, QByteArray>> &files)
{
    QByteArray out;
    QByteArray central;
    QDataStream body(&out, QIODevice::WriteOnly);
    body.setByteOrder(QDataStream::LittleEndian);
    QDataStream dir(&central, QIODevice::WriteOnly);
    dir.setByteOrder(QDataStream::LittleEndian);

    /* An MS-DOS date and time stamp: now. */
    const QDateTime now = QDateTime::currentDateTime();
    const quint16 dosTime = quint16((now.time().hour() << 11) | (now.time().minute() << 5)
                                    | (now.time().second() / 2));
    const quint16 dosDate = quint16(((qMax(1980, now.date().year()) - 1980) << 9)
                                    | (now.date().month() << 5) | now.date().day());

    for (const QPair<QString, QByteArray> &f : files)
    {
        const QByteArray name = f.first.toUtf8();
        const quint32 crc = crc32(f.second);
        const quint32 size = quint32(f.second.size());
        const quint32 offset = quint32(out.size());

        body << quint32(0x04034b50) << quint16(20) << quint16(0x0800)   // UTF-8 names
             << quint16(0) << dosTime << dosDate << crc << size << size
             << quint16(name.size()) << quint16(0);
        body.writeRawData(name.constData(), name.size());
        body.writeRawData(f.second.constData(), f.second.size());

        dir << quint32(0x02014b50) << quint16(20) << quint16(20) << quint16(0x0800)
            << quint16(0) << dosTime << dosDate << crc << size << size
            << quint16(name.size()) << quint16(0) << quint16(0) << quint16(0) << quint16(0)
            << quint32(0) << offset;
        dir.writeRawData(name.constData(), name.size());
    }

    const quint32 dirOffset = quint32(out.size());
    body.writeRawData(central.constData(), central.size());
    body << quint32(0x06054b50) << quint16(0) << quint16(0) << quint16(files.size())
         << quint16(files.size()) << quint32(central.size()) << dirOffset << quint16(0);
    return out;
}

QString XlsxWriter::columnName(int index)
{
    QString name;
    int n = index + 1;
    while (n > 0)
    {
        const int r = (n - 1) % 26;
        name.prepend(QChar('A' + r));
        n = (n - 1) / 26;
    }
    return name;
}

/* Text for XML: escaped, and without the control characters XML forbids. */
static QString xmlText(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar ch : text)
    {
        const ushort u = ch.unicode();
        if (u < 0x20 && u != '\t' && u != '\n' && u != '\r')
            continue;
        switch (u)
        {
            case '&': out += QLatin1String("&amp;"); break;
            case '<': out += QLatin1String("&lt;"); break;
            case '>': out += QLatin1String("&gt;"); break;
            case '"': out += QLatin1String("&quot;"); break;
            default: out += ch; break;
        }
    }
    return out;
}

QByteArray XlsxWriter::workbook(const QString &sheetName, const QList<QStringList> &rows,
                                const QList<int> &numericColumns)
{
    const QByteArray xmlHead = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

    QString sheet;
    sheet += QStringLiteral("<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">");
    if (!rows.isEmpty())
        sheet += QStringLiteral("<sheetViews><sheetView workbookViewId=\"0\"><pane ySplit=\"1\" "
                                "topLeftCell=\"A2\" activePane=\"bottomLeft\" state=\"frozen\"/>"
                                "</sheetView></sheetViews>");
    sheet += QStringLiteral("<sheetData>");
    for (int r = 0; r < rows.size(); ++r)
    {
        sheet += QStringLiteral("<row r=\"%1\">").arg(r + 1);
        const QStringList &line = rows.at(r);
        for (int c = 0; c < line.size(); ++c)
        {
            const QString &v = line.at(c);
            if (v.isEmpty())
                continue;
            const QString ref = columnName(c) + QString::number(r + 1);
            const QString style = r == 0 ? QStringLiteral(" s=\"1\"") : QString();
            bool isNumber = false;
            if (r > 0 && numericColumns.contains(c))
                v.toDouble(&isNumber);
            if (isNumber)
                sheet += QStringLiteral("<c r=\"%1\"%2><v>%3</v></c>").arg(ref, style, v);
            else
                sheet += QStringLiteral("<c r=\"%1\"%2 t=\"inlineStr\"><is><t xml:space=\"preserve\">%3</t></is></c>")
                             .arg(ref, style, xmlText(v));
        }
        sheet += QStringLiteral("</row>");
    }
    sheet += QStringLiteral("</sheetData></worksheet>");

    /* A sheet's name: 31 characters at most, none of []:*?/\ */
    QString name = sheetName;
    static const QString bad = QStringLiteral("[]:*?/\\");
    for (QChar &ch : name)
    {
        if (bad.contains(ch))
            ch = QLatin1Char('-');
    }
    name = name.left(31);
    if (name.isEmpty())
        name = QStringLiteral("Sheet1");

    QList<QPair<QString, QByteArray>> files;
    files << qMakePair(QStringLiteral("[Content_Types].xml"), xmlHead +
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "</Types>");
    files << qMakePair(QStringLiteral("_rels/.rels"), xmlHead +
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>");
    files << qMakePair(QStringLiteral("xl/workbook.xml"), xmlHead +
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"" + xmlText(name).toUtf8() + "\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
        "</workbook>");
    files << qMakePair(QStringLiteral("xl/_rels/workbook.xml.rels"), xmlHead +
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>");
    files << qMakePair(QStringLiteral("xl/styles.xml"), xmlHead +
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"2\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/></cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
        "</styleSheet>");
    files << qMakePair(QStringLiteral("xl/worksheets/sheet1.xml"), xmlHead + sheet.toUtf8());
    return zip(files);
}
