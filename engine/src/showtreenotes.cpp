/*
  Q Light Controller Plus
  showtreenotes.cpp

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

/* Notes beside the Show Tree (Q212). Dated entries on a production, a list,
   a cue, a layer or a palette, each with an optional snapshot of the view.
   They live in the tree's state, so each edit is a step on the tree's undo
   history; the snapshot pictures are kept apart (by a hash of their bytes:
   a picture never changes), so an undo step never carries them and an edit
   of the tree never copies them.

   A cue's own note (Cue::note, on the cue page, the cue sheet, the chaser
   step) is its first entry: an entry marked cueNote whose text is the
   note's. Needs work is "the cue has an open note": ticking the last one
   done clears it, setting it adds an open note "Needs work". Both are kept
   in step in syncCueNotes() after every tree edit and on load, so an older
   show's notes and marks come in as entries. */

#include <QCryptographicHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <algorithm>

#include "showtreecontroller.h"
#include "showtreenotes.h"
#include "showreview.h"
#include "qlcpalette.h"
#include "doc.h"

using namespace ShowTree;

/****************************************************************************
 * The model
 ****************************************************************************/

QString ShowTree::noteKindToString(NoteKind kind)
{
    switch (kind)
    {
        case NoteKind::Production: return QStringLiteral("Production");
        case NoteKind::List: return QStringLiteral("List");
        case NoteKind::Cue: return QStringLiteral("Cue");
        case NoteKind::Layer: return QStringLiteral("Layer");
        case NoteKind::Palette: return QStringLiteral("Palette");
    }
    return QStringLiteral("Cue");
}

NoteKind ShowTree::stringToNoteKind(const QString &str, bool *ok)
{
    static const NoteKind all[] = { NoteKind::Production, NoteKind::List, NoteKind::Cue,
                                    NoteKind::Layer, NoteKind::Palette };
    for (NoteKind k : all)
        if (noteKindToString(k) == str)
        {
            if (ok != nullptr)
                *ok = true;
            return k;
        }
    if (ok != nullptr)
        *ok = false;
    return NoteKind::Cue;
}

bool NoteView::operator==(const NoteView &o) const
{
    return valid == o.valid && plane == o.plane && rotation == o.rotation && qFuzzyCompare(azimuth, o.azimuth)
           && qFuzzyCompare(elevation, o.elevation) && zoomed == o.zoomed && qFuzzyCompare(scale, o.scale)
           && qFuzzyCompare(1.0 + originX, 1.0 + o.originX) && qFuzzyCompare(1.0 + originY, 1.0 + o.originY)
           && width == o.width && height == o.height && cue == o.cue && cueLabel == o.cueLabel
           && image == o.image;
}

bool Note::operator==(const Note &o) const
{
    return id == o.id && kind == o.kind && target == o.target && cue == o.cue && text == o.text
           && author == o.author && when == o.when && done == o.done && cueNote == o.cueNote && view == o.view;
}

bool ShowTree::noteNewerThan(const Note &a, const Note &b)
{
    // Undated (a cue note from before notes) sorts as the oldest.
    if (a.when != b.when)
        return a.when > b.when;
    return a.id > b.id;
}

void ShowTree::saveXML(QXmlStreamWriter *doc, const Note &note)
{
    doc->writeStartElement(QStringLiteral("Note"));
    doc->writeAttribute(QStringLiteral("ID"), QString::number(note.id));
    doc->writeAttribute(QStringLiteral("Kind"), noteKindToString(note.kind));
    doc->writeAttribute(QStringLiteral("Target"), QString::number(note.target));
    if (note.kind == NoteKind::Layer)
        doc->writeAttribute(QStringLiteral("Cue"), QString::number(note.cue));
    if (!note.author.isEmpty())
        doc->writeAttribute(QStringLiteral("By"), note.author);
    if (!note.when.isEmpty())
        doc->writeAttribute(QStringLiteral("When"), note.when);
    if (note.done)
        doc->writeAttribute(QStringLiteral("Done"), QStringLiteral("1"));
    if (note.cueNote)
        doc->writeAttribute(QStringLiteral("CueNote"), QStringLiteral("1"));
    doc->writeTextElement(QStringLiteral("Text"), note.text);
    if (note.view.valid)
    {
        const NoteView &v = note.view;
        doc->writeStartElement(QStringLiteral("View"));
        doc->writeAttribute(QStringLiteral("Plane"), QString::number(v.plane));
        doc->writeAttribute(QStringLiteral("Rotation"), QString::number(v.rotation));
        doc->writeAttribute(QStringLiteral("Azimuth"), QString::number(v.azimuth, 'g', 10));
        doc->writeAttribute(QStringLiteral("Elevation"), QString::number(v.elevation, 'g', 10));
        if (v.zoomed)
        {
            doc->writeAttribute(QStringLiteral("Zoomed"), QStringLiteral("1"));
            doc->writeAttribute(QStringLiteral("Scale"), QString::number(v.scale, 'g', 10));
            doc->writeAttribute(QStringLiteral("OriginX"), QString::number(v.originX, 'g', 10));
            doc->writeAttribute(QStringLiteral("OriginY"), QString::number(v.originY, 'g', 10));
        }
        doc->writeAttribute(QStringLiteral("Width"), QString::number(v.width));
        doc->writeAttribute(QStringLiteral("Height"), QString::number(v.height));
        if (v.cue != invalidId())
        {
            doc->writeAttribute(QStringLiteral("OnStage"), QString::number(v.cue));
            doc->writeAttribute(QStringLiteral("OnStageLabel"), v.cueLabel);
        }
        if (!v.image.isEmpty())
            doc->writeAttribute(QStringLiteral("Image"), v.image);
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

bool ShowTree::loadXML(QXmlStreamReader &root, Note &note)
{
    const QXmlStreamAttributes a = root.attributes();
    bool ok = false;
    note.id = a.value(QStringLiteral("ID")).toString().toUInt(&ok);
    bool kindOk = false;
    note.kind = stringToNoteKind(a.value(QStringLiteral("Kind")).toString(), &kindOk);
    bool targetOk = false;
    note.target = a.value(QStringLiteral("Target")).toString().toUInt(&targetOk);
    note.cue = a.hasAttribute(QStringLiteral("Cue")) ? a.value(QStringLiteral("Cue")).toString().toUInt()
                                                     : invalidId();
    note.author = a.value(QStringLiteral("By")).toString();
    note.when = a.value(QStringLiteral("When")).toString();
    note.done = a.value(QStringLiteral("Done")).toString() == QLatin1String("1");
    note.cueNote = a.value(QStringLiteral("CueNote")).toString() == QLatin1String("1");
    while (root.readNextStartElement())
    {
        if (root.name() == QLatin1String("Text"))
        {
            note.text = root.readElementText();
        }
        else if (root.name() == QLatin1String("View"))
        {
            const QXmlStreamAttributes v = root.attributes();
            NoteView &view = note.view;
            view.valid = true;
            view.plane = v.value(QStringLiteral("Plane")).toString().toInt();
            view.rotation = v.value(QStringLiteral("Rotation")).toString().toInt();
            view.azimuth = v.value(QStringLiteral("Azimuth")).toString().toDouble();
            view.elevation = v.value(QStringLiteral("Elevation")).toString().toDouble();
            view.zoomed = v.value(QStringLiteral("Zoomed")).toString() == QLatin1String("1");
            if (view.zoomed)
            {
                view.scale = v.value(QStringLiteral("Scale")).toString().toDouble();
                view.originX = v.value(QStringLiteral("OriginX")).toString().toDouble();
                view.originY = v.value(QStringLiteral("OriginY")).toString().toDouble();
            }
            view.width = v.value(QStringLiteral("Width")).toString().toInt();
            view.height = v.value(QStringLiteral("Height")).toString().toInt();
            if (v.hasAttribute(QStringLiteral("OnStage")))
            {
                view.cue = v.value(QStringLiteral("OnStage")).toString().toUInt();
                view.cueLabel = v.value(QStringLiteral("OnStageLabel")).toString();
            }
            view.image = v.value(QStringLiteral("Image")).toString();
            root.skipCurrentElement();
        }
        else
        {
            root.skipCurrentElement();
        }
    }
    return ok && kindOk && targetOk;
}

/****************************************************************************
 * Reading them
 ****************************************************************************/

QList<Note> ShowTreeController::notes(NoteKind kind, quint32 target) const
{
    QList<Note> out;
    for (const Note &n : m_notes)
        if (n.kind == kind && n.target == target)
            out << n;
    std::sort(out.begin(), out.end(), noteNewerThan);
    return out;
}

QList<Note> ShowTreeController::notesOfCue(quint32 cueId) const
{
    QList<Note> layers;
    for (const Note &n : m_notes)
        if (n.kind == NoteKind::Layer && n.cue == cueId && noteTargetExists(n))
            layers << n;
    std::sort(layers.begin(), layers.end(), noteNewerThan);
    return notes(NoteKind::Cue, cueId) + layers;
}

QList<Note> ShowTreeController::allNotes() const
{
    QList<Note> out;
    for (const Note &n : m_notes)
        if (noteTargetExists(n))
            out << n;
    std::sort(out.begin(), out.end(), noteNewerThan);
    return out;
}

const Note *ShowTreeController::note(quint32 noteId) const
{
    for (const Note &n : m_notes)
        if (n.id == noteId)
            return &n;
    return nullptr;
}

int ShowTreeController::noteCount(NoteKind kind, quint32 target, int *open) const
{
    int all = 0, opened = 0;
    for (const Note &n : m_notes)
        if (n.kind == kind && n.target == target)
        {
            ++all;
            if (!n.done)
                ++opened;
        }
    if (open != nullptr)
        *open = opened;
    return all;
}

const Note *ShowTreeController::latestNote(NoteKind kind, quint32 target) const
{
    const Note *best = nullptr;
    for (const Note &n : m_notes)
        if (n.kind == kind && n.target == target && (best == nullptr || noteNewerThan(n, *best)))
            best = &n;
    return best;
}

bool ShowTreeController::noteTargetExists(const Note &n) const
{
    switch (n.kind)
    {
        case NoteKind::Production: return production(n.target) != nullptr;
        case NoteKind::List: return cueList(n.target) != nullptr;
        case NoteKind::Cue: return cue(n.target) != nullptr;
        case NoteKind::Layer:
        {
            const Cue *c = cue(n.cue);
            return c != nullptr && c->layer(n.target) != nullptr;
        }
        case NoteKind::Palette: return m_doc != nullptr && m_doc->palette(n.target) != nullptr;
    }
    return false;
}

QString ShowTreeController::noteTargetLabel(const Note &n) const
{
    auto cueText = [this](quint32 cueId) {
        const CueList *l = listOfCue(cueId);
        return l == nullptr ? QString() : cueLabel(*l, l->indexOf(cueId));
    };
    switch (n.kind)
    {
        case NoteKind::Production:
            if (const Production *p = production(n.target))
                return tr("Production %1").arg(p->name);
        break;
        case NoteKind::List:
            if (const CueList *l = cueList(n.target))
                return tr("List %1").arg(l->name);
        break;
        case NoteKind::Cue:
            if (const CueList *l = listOfCue(n.target))
                return (l->looks ? tr("Hit %1") : tr("Cue %1")).arg(cueText(n.target));
        break;
        case NoteKind::Layer:
            if (const Cue *c = cue(n.cue))
                if (const Layer *layer = c->layer(n.target))
                    return tr("Layer %1 (cue %2)")
                        .arg(layer->name.isEmpty() ? targetLabel(layer->target) : layer->name, cueText(n.cue));
        break;
        case NoteKind::Palette:
            if (const QLCPalette *p = m_doc != nullptr ? m_doc->palette(n.target) : nullptr)
                return tr("Palette %1").arg(p->name());
        break;
    }
    return tr("(deleted)");
}

/****************************************************************************
 * Editing them
 ****************************************************************************/

bool ShowTreeController::editNotes(const QString &label,
                                   const std::function<bool(QList<Note> &, QList<Production> &)> &change)
{
    const bool batch = m_batchDepth > 0;
    const QByteArray before = batch ? QByteArray() : snapshotXml();
    QList<Production> work = m_productions;
    QList<Note> notes = m_notes;
    if (!change(notes, work))
        return false;
    m_notes = notes;
    syncNeedsWork(work);

    // Only a cue's own note plays a part in what is baked (its chaser
    // step's note); Needs work is a mark and bakes nothing. The change
    // never adds or removes a cue, so the two trees line up.
    bool cueNoteChanged = false, treeChanged = false;
    for (int p = 0; p < work.count(); ++p)
        for (int l = 0; l < work.at(p).lists.count(); ++l)
            for (int c = 0; c < work.at(p).lists.at(l).cues.count(); ++c)
            {
                const Cue &now = work.at(p).lists.at(l).cues.at(c);
                const Cue &was = m_productions.at(p).lists.at(l).cues.at(c);
                if (now.note != was.note)
                    cueNoteChanged = true;
                if (now.needsWork != was.needsWork)
                    treeChanged = true;
            }
    treeChanged = treeChanged || cueNoteChanged;
    m_productions = work;

    if (batch)
    {
        m_batchChanged = true;
        return true;
    }
    pushUndo(label, before);
    if (cueNoteChanged)
        bake();
    m_doc->setModified();
    if (treeChanged)
        emit changed();
    emit notesChanged();
    return true;
}

void ShowTreeController::syncNeedsWork(QList<Production> &ps) const
{
    QSet<quint32> open;
    for (const Note &n : m_notes)
        if (n.kind == NoteKind::Cue && !n.done)
            open.insert(n.target);
    for (Production &p : ps)
        for (CueList &l : p.lists)
            for (Cue &c : l.cues)
                c.needsWork = open.contains(c.id);
}

void ShowTreeController::syncCueNotes(QList<Production> &ps)
{
    QHash<quint32, int> cueNoteAt;
    QSet<quint32> open;
    for (int i = 0; i < m_notes.count(); ++i)
    {
        const Note &n = m_notes.at(i);
        if (n.kind != NoteKind::Cue)
            continue;
        if (n.cueNote && !cueNoteAt.contains(n.target))
            cueNoteAt.insert(n.target, i);
        if (!n.done)
            open.insert(n.target);
    }
    QSet<int> drop;
    QList<Note> added;
    for (Production &p : ps)
        for (CueList &l : p.lists)
            for (Cue &c : l.cues)
            {
                const int at = cueNoteAt.value(c.id, -1);
                if (!c.note.isEmpty())
                {
                    if (at < 0)
                    {
                        // An older show's note, or a cue copied with one:
                        // its first entry, undated.
                        Note n;
                        n.id = m_nextId++;
                        n.kind = NoteKind::Cue;
                        n.target = c.id;
                        n.text = c.note;
                        n.cueNote = true;
                        // Needs work said so: the note is what needs doing.
                        n.done = !c.needsWork;
                        if (c.needsWork)
                            open.insert(c.id);
                        added << n;
                    }
                    else if (m_notes.at(at).text != c.note)
                    {
                        m_notes[at].text = c.note;
                    }
                }
                else if (at >= 0)
                {
                    drop.insert(at);
                }

                if (c.needsWork && !open.contains(c.id))
                {
                    Note n;
                    n.id = m_nextId++;
                    n.kind = NoteKind::Cue;
                    n.target = c.id;
                    n.text = tr("Needs work");
                    n.author = ShowReview::defaultName();
                    n.when = QDateTime::currentDateTime().toString(Qt::ISODate);
                    added << n;
                    open.insert(c.id);
                }
                else if (!c.needsWork && open.contains(c.id))
                {
                    c.needsWork = true;
                }
            }
    if (!drop.isEmpty())
    {
        QList<Note> kept;
        for (int i = 0; i < m_notes.count(); ++i)
            if (!drop.contains(i))
                kept << m_notes.at(i);
        m_notes = kept;
    }
    m_notes << added;
}

quint32 ShowTreeController::addNote(NoteKind kind, quint32 target, const QString &textIn, const NoteView &view,
                                    quint32 layerCue, const QString &author)
{
    const QString text = textIn.trimmed();
    if (text.isEmpty() && !view.valid)
        return invalidId();
    Note n;
    n.kind = kind;
    n.target = target;
    n.cue = kind == NoteKind::Layer ? layerCue : invalidId();
    n.text = text;
    n.view = view;
    if (!noteTargetExists(n))
        return invalidId();
    n.author = author.trimmed().isEmpty() ? ShowReview::defaultName() : author.trimmed();
    n.when = QDateTime::currentDateTime().toString(Qt::ISODate);
    n.id = m_nextId;
    const bool ok = editNotes(tr("Add note"), [&](QList<Note> &notes, QList<Production> &) {
        ++m_nextId;
        notes << n;
        return true;
    });
    return ok ? n.id : invalidId();
}

bool ShowTreeController::setNoteText(quint32 noteId, const QString &textIn)
{
    const Note *have = note(noteId);
    const QString text = textIn.trimmed();
    if (have == nullptr || have->text == text)
        return false;
    // A cue's own note emptied: the entry goes with it (as clearing the note
    // on the cue page does). Any other note keeps its place, empty or not.
    if (text.isEmpty() && have->cueNote)
        return removeNote(noteId);
    return editNotes(tr("Edit note"), [&](QList<Note> &notes, QList<Production> &ps) {
        for (Note &n : notes)
            if (n.id == noteId)
            {
                n.text = text;
                if (n.cueNote)
                    for (Production &p : ps)
                        for (CueList &l : p.lists)
                            for (Cue &c : l.cues)
                                if (c.id == n.target)
                                    c.note = text;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::setNoteDone(quint32 noteId, bool done)
{
    const Note *have = note(noteId);
    if (have == nullptr || have->done == done)
        return false;
    return editNotes(done ? tr("Note done") : tr("Note open again"), [&](QList<Note> &notes, QList<Production> &) {
        for (Note &n : notes)
            if (n.id == noteId)
            {
                n.done = done;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::setNoteView(quint32 noteId, const NoteView &view)
{
    const Note *have = note(noteId);
    if (have == nullptr || have->view == view)
        return false;
    return editNotes(view.valid ? tr("Snapshot the view") : tr("Remove snapshot"),
                     [&](QList<Note> &notes, QList<Production> &) {
        for (Note &n : notes)
            if (n.id == noteId)
            {
                n.view = view;
                return true;
            }
        return false;
    });
}

bool ShowTreeController::removeNote(quint32 noteId)
{
    if (note(noteId) == nullptr)
        return false;
    return editNotes(tr("Delete note"), [&](QList<Note> &notes, QList<Production> &ps) {
        for (int i = 0; i < notes.count(); ++i)
            if (notes.at(i).id == noteId)
            {
                const Note n = notes.takeAt(i);
                if (n.cueNote)
                    for (Production &p : ps)
                        for (CueList &l : p.lists)
                            for (Cue &c : l.cues)
                                if (c.id == n.target)
                                    c.note.clear();
                return true;
            }
        return false;
    });
}

/****************************************************************************
 * Pictures
 ****************************************************************************/

QString ShowTreeController::addNoteImage(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return QString();
    const QString id = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex().left(20));
    m_noteImages.insert(id, bytes);
    return id;
}

QByteArray ShowTreeController::noteImage(const QString &id) const
{
    return m_noteImages.value(id);
}

qint64 ShowTreeController::noteImageBytes() const
{
    QSet<QString> used;
    for (const Note &n : m_notes)
        if (!n.view.image.isEmpty() && noteTargetExists(n))
            used.insert(n.view.image);
    qint64 total = 0;
    for (const QString &id : used)
        total += m_noteImages.value(id).size();
    return total;
}

/****************************************************************************
 * Load & save
 ****************************************************************************/

void ShowTreeController::saveNotes(QXmlStreamWriter *doc, bool snapshot) const
{
    // An undo step keeps every note, a deleted cue's too (its undo brings
    // them back); the file keeps those on what is still there.
    QList<const Note *> kept;
    for (const Note &n : m_notes)
        if (snapshot || noteTargetExists(n))
            kept << &n;
    if (kept.isEmpty())
        return;
    doc->writeStartElement(QStringLiteral("Notes"));
    for (const Note *n : kept)
        ShowTree::saveXML(doc, *n);
    doc->writeEndElement();
    if (snapshot)
        return;

    QStringList images;
    for (const Note *n : kept)
        if (!n->view.image.isEmpty() && m_noteImages.contains(n->view.image) && !images.contains(n->view.image))
            images << n->view.image;
    std::sort(images.begin(), images.end());
    for (const QString &id : images)
    {
        doc->writeStartElement(QStringLiteral("NoteImage"));
        doc->writeAttribute(QStringLiteral("ID"), id);
        doc->writeCharacters(QString::fromLatin1(m_noteImages.value(id).toBase64()));
        doc->writeEndElement();
    }
}

void ShowTreeController::loadNotes(QXmlStreamReader &root)
{
    QSet<quint32> ids;
    while (root.readNextStartElement())
    {
        if (root.name() != QLatin1String("Note"))
        {
            root.skipCurrentElement();
            continue;
        }
        Note n;
        if (ShowTree::loadXML(root, n) && !ids.contains(n.id))
        {
            ids.insert(n.id);
            m_notes << n;
        }
    }
}

void ShowTreeController::loadNoteImage(QXmlStreamReader &root)
{
    const QString id = root.attributes().value(QStringLiteral("ID")).toString();
    const QByteArray bytes = QByteArray::fromBase64(root.readElementText().toLatin1());
    if (!id.isEmpty() && !bytes.isEmpty())
        m_noteImages.insert(id, bytes);
}
