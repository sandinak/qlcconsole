/*
  Q Light Controller Plus
  fixturecreator.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QRegularExpression>
#include <QMessageBox>
#include <QSet>

#include "fixturecreator.h"
#include "inputoutputmap.h"
#include "qlcfixturedef.h"
#include "qlcfixturemode.h"
#include "fixturegroup.h"
#include "addfixture.h"
#include "apputil.h"
#include "fixture.h"
#include "doc.h"

QList<quint32> FixtureCreator::addFromDialog(QWidget *parent, Doc *doc, bool inventory,
                                             quint32 preselectUniverse, FixtureGroup *addToGroup,
                                             const Fixture *like, const QString &manufacturer)
{
    QList<quint32> created;
    AddFixture af(parent, doc);
    if (like != NULL)
        af.preselectLike(like);
    else
        af.preselectManufacturer(manufacturer);
    af.setInventoryMode(inventory);
    // Seed the dialog with the universe the context menu was opened in, so
    // right-clicking inside a universe adds there instead of the global default.
    if (!inventory && preselectUniverse != InputOutputMap::invalidUniverse())
        af.preselectUniverse(preselectUniverse);
    if (af.exec() == QDialog::Rejected)
        return created;

    if (af.invalidAddress())
    {
        QMessageBox msg(QMessageBox::Critical, tr("Error"),
                tr("Please enter a valid address"), QMessageBox::Ok);
        msg.exec();
        return created;
    }

    QString name = af.name();
    quint32 address = af.address();
    quint32 universe = af.universe();
    quint32 channels = af.channels();
    int gap = af.gap();

    QLCFixtureDef* fixtureDef = af.fixtureDef();
    QLCFixtureMode* mode = af.mode();

    /* If an empty name was given use the model instead */
    if (name.simplified().isEmpty())
    {
        if (fixtureDef != NULL)
            name = fixtureDef->model();
        else
            name = tr("Generic Dimmer");
    }

    /* Names already in use, so a multi-add continues past existing fixtures
       and never collides. Seeded once and grown as we assign each name.
       A single add gets the next free "name #2" if the name is taken. */
    QSet<QString> usedNames;
    bool nameHasTrailingNumber = false;
    if (af.amount() > 1)
    {
        for (Fixture *ef : doc->fixtures())
            if (ef != NULL)
                usedNames.insert(ef->name());
        // If the typed name already ends in an explicit counter ("US #4"), treat
        // it as the start of a run and INCREMENT that number (US #4, US #5, …)
        // rather than bolting a second "#NN" counter onto it ("US #4 #1").
        // Only a "#N" suffix counts: trailing digits alone are usually part of a
        // model ("LM70", "Par 64"), and bumping those (LM71, LM72) invents
        // models that do not exist.
        nameHasTrailingNumber =
            QRegularExpression(QStringLiteral("#\\d+$")).match(name.trimmed()).hasMatch();
    }

    /* Add the rest (if any) WITH address gap */
    for (int i = 0; i < af.amount(); i++)
    {
        QString modname;

        /* If we're adding more than one fixture, derive a unique name */
        if (af.amount() > 1)
        {
            // Numbered base: hand nextUniqueName the SAME name each pass — the
            // accumulating usedNames set makes it land on the next free number
            // (US #4 → US #5 → US #6). Unnumbered base: classic "Name #NN".
            const QString base = nameHasTrailingNumber
                ? name
                : QString("%1 #%2").arg(name)
                      .arg(i + 1, AppUtil::digits(af.amount()), 10, QChar('0'));
            modname = Doc::nextUniqueName(base, [&usedNames](const QString &c) {
                return usedNames.contains(c);
            });
            usedNames.insert(modname);
        }
        else
            modname = doc->uniqueFixtureName(name);   // names are unique: "X #2"

        /* Create the fixture */
        Fixture* fxi = new Fixture(doc);

        /* Assign the next address AFTER the previous fixture
           address space plus gap. */
        if (inventory)
        {
            // Staged, not patched: the 0/0 is a placeholder that Doc::addFixture()
            // never registers and no DMX path ever writes.
            fxi->setPatched(false);
            fxi->setAddress(0);
            fxi->setUniverse(0);
        }
        else
        {
            fxi->setAddress(address + (i * channels) + (i * gap));
            fxi->setUniverse(universe);
        }
        fxi->setConnection(af.connection());
        /* Each network fixture is its own device: a batch gets one IP each,
           counting up from the one given (.60, .61, ...). */
        fxi->setIPAddress(af.amount() > 1 && !af.ipAddress().isEmpty()
                          ? AppUtil::nthAddress(af.ipAddress(), i) : af.ipAddress());
        fxi->setTransport(af.transport());
        fxi->setName(modname);
        /* Set a fixture definition & mode if they were
           selected. Otherwise create a fixture definition
           and mode for a generic dimmer. */
        if (fixtureDef != NULL && mode != NULL)
        {
            fxi->setFixtureDefinition(fixtureDef, mode);
        }
        else
        {
            QLCFixtureDef* genericDef = fxi->genericDimmerDef(channels);
            QLCFixtureMode* genericMode = fxi->genericDimmerMode(genericDef, channels);
            fxi->setFixtureDefinition(genericDef, genericMode);
        }

        if (doc->addFixture(fxi))
        {
            created << fxi->id();
            if (addToGroup != NULL)
                addToGroup->assignFixture(fxi->id());
        }
        else
        {
            delete fxi;
        }
    }

    return created;
}

