/*
  Q Light Controller Plus
  stringanchorsdialog.cpp

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

#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>

#include "stringanchorsdialog.h"
#include "lengthspinbox.h"
#include "pixelpath.h"

namespace
{
enum Column { AnchorCol = 0, AcrossCol, DepthCol, HeightCol, PinCol, ReverseCol, Columns };

LengthSpinBox *metresField(QWidget *parent, double lo, double hi, double value)
{
    // Metres kept; shown and typed in Settings > Units.
    LengthSpinBox *f = new LengthSpinBox(1000.0, parent);
    f->setDecimals(4);
    f->setRange(lo, hi);
    f->setSingleStep(0.0254);
    f->setValue(value);
    return f;
}
}

StringAnchorsDialog::StringAnchorsDialog(const QVector<StringAnchor> &anchors, int leds, float height,
                                         QWidget *parent)
    : QDialog(parent)
    , m_leds(leds)
    , m_height(height)
{
    setObjectName(QStringLiteral("stringAnchorsDialog"));
    setWindowTitle(tr("String Anchors"));
    setProperty("helpTopic", QStringLiteral("lighting-studio.md#free-string-anchors"));

    QLabel *intro = new QLabel(tr("One row per anchor, in the order the string runs from its first "
                                  "LED: where it is across the stage and up or down it (as on the "
                                  "plot's rulers), and how high. Pin an LED to say which one is at "
                                  "the anchor; the LEDs between two pins are spread evenly. Reverse "
                                  "turns the span from that anchor to the next round when its "
                                  "segments are written to the WLED."), this);
    intro->setWordWrap(true);

    m_table = new QTableWidget(0, Columns, this);
    m_table->setObjectName(QStringLiteral("stringAnchorsTable"));
    m_table->setHorizontalHeaderLabels(QStringList() << tr("Anchor") << tr("Across") << tr("Depth")
                                                     << tr("Height") << tr("Pinned LED") << tr("Reverse"));
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(AnchorCol, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    foreach (const StringAnchor &a, anchors)
        addRow(a);

    QPushButton *add = new QPushButton(tr("Add Anchor"), this);
    add->setObjectName(QStringLiteral("stringAnchorsAdd"));
    add->setToolTip(tr("A new anchor after the last, %1 on the way the string was going")
                       .arg(MeasurementUnits::formatLength(304.8)));
    QPushButton *remove = new QPushButton(tr("Remove"), this);
    remove->setObjectName(QStringLiteral("stringAnchorsRemove"));
    connect(add, &QPushButton::clicked, this, [this]() {
        const QVector<StringAnchor> now = this->anchors();
        StringAnchor a;
        a.z = m_height;
        if (!now.isEmpty())
        {
            a = now.last();
            a.pin = 0;
            a.reverse = false;
            QVector3D dir(1, 0, 0);
            if (now.size() > 1 && (now.last().pos() - now.at(now.size() - 2).pos()).length() > 1e-4f)
                dir = (now.last().pos() - now.at(now.size() - 2).pos()).normalized();
            const QVector3D p = now.last().pos() + dir * 0.3048f;
            a.x = p.x(); a.y = p.y(); a.z = p.z();
        }
        addRow(a);
        m_table->setCurrentCell(m_table->rowCount() - 1, AcrossCol);
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        const int r = m_table->currentRow();
        if (r >= 0)
        {
            m_table->removeRow(r);
            renumber();
        }
    });
    QHBoxLayout *rowBtns = new QHBoxLayout;
    rowBtns->addWidget(add);
    rowBtns->addWidget(remove);
    rowBtns->addStretch();

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("stringAnchorsError"));
    m_error->setStyleSheet(QStringLiteral("color:#e06060;"));
    m_error->setWordWrap(true);
    m_error->hide();

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(bb, &QDialogButtonBox::accepted, this, &StringAnchorsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *vl = new QVBoxLayout(this);
    vl->addWidget(intro);
    vl->addWidget(m_table, 1);
    vl->addLayout(rowBtns);
    vl->addWidget(m_error);
    vl->addWidget(bb);
    resize(640, 380);
}

void StringAnchorsDialog::addRow(const StringAnchor &a)
{
    const int r = m_table->rowCount();
    m_table->insertRow(r);
    QTableWidgetItem *num = new QTableWidgetItem;
    num->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_table->setItem(r, AnchorCol, num);
    m_table->setCellWidget(r, AcrossCol, metresField(m_table, -1000.0, 1000.0, double(a.x)));
    m_table->setCellWidget(r, DepthCol, metresField(m_table, -1000.0, 1000.0, double(a.y)));
    m_table->setCellWidget(r, HeightCol, metresField(m_table, -50.0, 100.0, double(a.z)));
    QSpinBox *pin = new QSpinBox(m_table);
    pin->setRange(0, m_leds > 0 ? m_leds : 100000);
    pin->setSpecialValueText(tr("None"));
    pin->setValue(qMax(0, a.pin));
    pin->setToolTip(tr("The LED tied at this anchor, counted from the start of the string (1 is "
                       "the first). None: wherever the even spread puts it"));
    m_table->setCellWidget(r, PinCol, pin);
    QCheckBox *rev = new QCheckBox(m_table);
    rev->setChecked(a.reverse);
    rev->setToolTip(tr("The span from this anchor to the next runs the other way on the WLED"));
    m_table->setCellWidget(r, ReverseCol, rev);
    renumber();
}

void StringAnchorsDialog::renumber()
{
    for (int r = 0; r < m_table->rowCount(); ++r)
        if (QTableWidgetItem *it = m_table->item(r, AnchorCol))
            it->setText(QString::number(r + 1));
}

QVector<StringAnchor> StringAnchorsDialog::anchors() const
{
    QVector<StringAnchor> out;
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        auto *x = qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(r, AcrossCol));
        auto *y = qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(r, DepthCol));
        auto *z = qobject_cast<QDoubleSpinBox *>(m_table->cellWidget(r, HeightCol));
        auto *pin = qobject_cast<QSpinBox *>(m_table->cellWidget(r, PinCol));
        auto *rev = qobject_cast<QCheckBox *>(m_table->cellWidget(r, ReverseCol));
        if (x == nullptr || y == nullptr || z == nullptr || pin == nullptr || rev == nullptr)
            continue;
        StringAnchor a;
        a.x = float(x->value());
        a.y = float(y->value());
        a.z = float(z->value());
        a.pin = pin->value();
        a.reverse = rev->isChecked();
        out << a;
    }
    return out;
}

QString StringAnchorsDialog::problem() const
{
    return PixelPath::checkStringAnchors(anchors(), m_leds);
}

void StringAnchorsDialog::accept()
{
    const QString why = problem();
    if (!why.isEmpty())
    {
        m_error->setText(why);
        m_error->show();
        return;
    }
    QDialog::accept();
}
