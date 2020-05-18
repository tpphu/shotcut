/*
 * Copyright (c) 2020 Meltytech, LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "slideshowgeneratorwidget.h"

#include "Logger.h"
#include "mltcontroller.h"
#include "settings.h"
#include "shotcut_mlt_properties.h"
#include "widgets/producerpreviewwidget.h"

#include <MltFilter.h>
#include <MltTransition.h>

#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QtConcurrent/QtConcurrent>

#include <math.h>

enum
{
    ASPECT_CONVERSION_PAD_BLACK = 0,
    ASPECT_CONVERSION_CROP_CENTER = 1,
    ASPECT_CONVERSION_CROP_PAN = 2,
};

SlideshowGeneratorWidget::SlideshowGeneratorWidget(Mlt::Playlist* clips, QWidget *parent)
    : QWidget(parent)
    , m_clips(clips)
    , m_refreshPreview(false)
    , m_previewProducer(nullptr)
{
    QGridLayout* grid = new QGridLayout();
    setLayout(grid);

    grid->addWidget(new QLabel(tr("Clip Duration")), 0, 0, Qt::AlignRight);
    m_clipDurationSpinner = new QSpinBox();
    m_clipDurationSpinner->setToolTip(tr("Set the duration of each clip in the slideshow."));
    m_clipDurationSpinner->setSuffix(" s");
    m_clipDurationSpinner->setMinimum(4);
    m_clipDurationSpinner->setMaximum(600);
    m_clipDurationSpinner->setValue(10);
    connect(m_clipDurationSpinner, SIGNAL(valueChanged(int)), this, SLOT(on_parameterChanged()));
    grid->addWidget(m_clipDurationSpinner, 0, 1);

    grid->addWidget(new QLabel(tr("Aspect Ratio Conversion")), 1, 0, Qt::AlignRight);
    m_aspectConversionCombo = new QComboBox();
    m_aspectConversionCombo->addItem(tr("Pad Black"));
    m_aspectConversionCombo->addItem(tr("Crop Center"));
    m_aspectConversionCombo->addItem(tr("Crop and Pan"));
    m_aspectConversionCombo->setToolTip(tr("Choose an aspect ratio conversion method."));
    m_aspectConversionCombo->setCurrentIndex(ASPECT_CONVERSION_CROP_CENTER);
    connect(m_aspectConversionCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_parameterChanged()));
    grid->addWidget(m_aspectConversionCombo, 1, 1);

    grid->addWidget(new QLabel(tr("Zoom Effect")), 2, 0, Qt::AlignRight);
    m_zoomPercentSpinner = new QSpinBox();
    m_zoomPercentSpinner->setToolTip(tr("Set the percentage of the zoom-in effect.\n0% will result in no zoom effect."));
    m_zoomPercentSpinner->setSuffix(" %");
    m_zoomPercentSpinner->setMinimum(0);
    m_zoomPercentSpinner->setMaximum(50);
    m_zoomPercentSpinner->setValue(10);
    connect(m_zoomPercentSpinner, SIGNAL(valueChanged(int)), this, SLOT(on_parameterChanged()));
    grid->addWidget(m_zoomPercentSpinner, 2, 1);

    grid->addWidget(new QLabel(tr("Transition Duration")), 3, 0, Qt::AlignRight);
    m_transitionDurationSpinner = new QSpinBox();
    m_transitionDurationSpinner->setToolTip(tr("Set the duration of the transition.\nMay not be longer than half the duration of the clip.\nIf the duration is 0, no transition will be created."));
    m_transitionDurationSpinner->setSuffix(" s");
    m_transitionDurationSpinner->setMinimum(0);
    m_transitionDurationSpinner->setMaximum(10);
    m_transitionDurationSpinner->setValue(2);
    connect(m_transitionDurationSpinner, SIGNAL(valueChanged(int)), this, SLOT(on_parameterChanged()));
    grid->addWidget(m_transitionDurationSpinner, 3, 1);

    grid->addWidget(new QLabel(tr("Transition Type")), 4, 0, Qt::AlignRight);
    m_transitionStyleCombo = new QComboBox();
    m_transitionStyleCombo->addItem(tr("Random"));
    m_transitionStyleCombo->addItem(tr("Dissolve"));
    m_transitionStyleCombo->addItem(tr("Bar Horizontal"));
    m_transitionStyleCombo->addItem(tr("Bar Vertical"));
    m_transitionStyleCombo->addItem(tr("Barn Door Horizontal"));
    m_transitionStyleCombo->addItem(tr("Barn Door Vertical"));
    m_transitionStyleCombo->addItem(tr("Barn Door Diagonal SW-NE"));
    m_transitionStyleCombo->addItem(tr("Barn Door Diagonal NW-SE"));
    m_transitionStyleCombo->addItem(tr("Diagonal Top Left"));
    m_transitionStyleCombo->addItem(tr("Diagonal Top Right"));
    m_transitionStyleCombo->addItem(tr("Matrix Waterfall Horizontal"));
    m_transitionStyleCombo->addItem(tr("Matrix Waterfall Vertical"));
    m_transitionStyleCombo->addItem(tr("Matrix Snake Horizontal"));
    m_transitionStyleCombo->addItem(tr("Matrix Snake Parallel Horizontal"));
    m_transitionStyleCombo->addItem(tr("Matrix Snake Vertical"));
    m_transitionStyleCombo->addItem(tr("Matrix Snake Parallel Vertical"));
    m_transitionStyleCombo->addItem(tr("Barn V Up"));
    m_transitionStyleCombo->addItem(tr("Iris Circle"));
    m_transitionStyleCombo->addItem(tr("Double Iris"));
    m_transitionStyleCombo->addItem(tr("Iris Box"));
    m_transitionStyleCombo->addItem(tr("Box Bottom Right"));
    m_transitionStyleCombo->addItem(tr("Box Bottom Left"));
    m_transitionStyleCombo->addItem(tr("Box Right Center"));
    m_transitionStyleCombo->addItem(tr("Clock Top"));
    m_transitionStyleCombo->setToolTip(tr("Choose a transition effect."));
    m_transitionStyleCombo->setCurrentIndex(1);
    connect(m_transitionStyleCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_parameterChanged()));
    grid->addWidget(m_transitionStyleCombo, 4, 1);

    grid->addWidget(new QLabel(tr("Transition Softness")), 5, 0, Qt::AlignRight);
    m_softnessSpinner = new QSpinBox();
    m_softnessSpinner->setToolTip(tr("Change the softness of the edge of the wipe."));
    m_softnessSpinner->setSuffix(" %");
    m_softnessSpinner->setMaximum(100);
    m_softnessSpinner->setMinimum(0);
    m_softnessSpinner->setValue(20);
    connect(m_softnessSpinner, SIGNAL(valueChanged(int)), this, SLOT(on_parameterChanged()));
    grid->addWidget(m_softnessSpinner, 5, 1);

    m_preview = new ProducerPreviewWidget(m_clips->profile()->dar());
    grid->addWidget(m_preview, 6, 0, 1, 2, Qt::AlignCenter);

    on_parameterChanged();
}

SlideshowGeneratorWidget::~SlideshowGeneratorWidget()
{
    m_future.waitForFinished();
    m_preview->stop();
    if(m_previewProducer)
    {
        delete m_previewProducer;
    }
}

Mlt::Playlist* SlideshowGeneratorWidget::getSlideshow()
{
    SlideshowConfig config;
    m_mutex.lock();
    // take a snapshot of the config.
    config = m_config;
    m_mutex.unlock();

    int framesPerClip = ceil((double)config.clipDuration * m_clips->profile()->fps());
    int count = m_clips->count();
    Mlt::Playlist* slideshow = new Mlt::Playlist(*m_clips->profile());
    Mlt::ClipInfo info;

    // Copy clips
    for (int i = 0; i < count; i++)
    {
        Mlt::ClipInfo* c = m_clips->clip_info(i, &info);
        if (c)
        {
            Mlt::Producer producer(*c->producer->profile(), "xml-string", MLT.XML(c->producer).toUtf8().constData());
            slideshow->append(producer, c->frame_in, c->frame_in + framesPerClip - 1);
        }
    }

    // Add filters
    if (config.zoomPercent > 0 || config.aspectConversion != ASPECT_CONVERSION_PAD_BLACK)
    {
        for (int i = 0; i < count; i++)
        {
            Mlt::ClipInfo* c = slideshow->clip_info(i, &info);
            if (c)
            {
                Mlt::Filter filter(*c->producer->profile(), "affine");
                applyAffineFilterProperties(&filter, config, c->producer, c->frame_in + framesPerClip - 1);
                c->producer->attach(filter);
            }
        }
    }

    // Add transitions
    int framesPerTransition = ceil((double)config.transitionDuration * m_clips->profile()->fps());
    if (framesPerTransition > (framesPerClip / 2 - 1))
    {
        framesPerTransition = (framesPerClip / 2 - 1);
    }
    if (framesPerTransition > 0)
    {
        for (int i = 0; i < count - 1; i++)
        {
            // Create playlist mix
            slideshow->mix(i, framesPerTransition);
            QScopedPointer<Mlt::Producer> producer(slideshow->get_clip(i + 1));
            if( producer.isNull() )
            {
                break;
            }
            producer->parent().set(kShotcutTransitionProperty, "lumaMix");

            // Add mix transition
            Mlt::Transition crossFade(*m_clips->profile(), "mix:-1");
            slideshow->mix_add(i + 1, &crossFade);

            // Add luma transition
            Mlt::Transition luma(*m_clips->profile(), Settings.playerGPU()? "movit.luma_mix" : "luma");
            applyLumaTransitionProperties(&luma, config);
            slideshow->mix_add(i + 1, &luma);

            count++;
            i++;
        }
    }

    return slideshow;
}

void SlideshowGeneratorWidget::applyAffineFilterProperties(Mlt::Filter* filter, SlideshowConfig& config, Mlt::Producer* producer, int endPosition)
{
    mlt_rect beginRect;
    mlt_rect endRect;
    beginRect.x = 0;
    beginRect.y = 0;
    beginRect.w = producer->profile()->width();
    beginRect.h = producer->profile()->height();
    beginRect.o = 1;
    endRect.x = beginRect.x;
    endRect.y = beginRect.y;
    endRect.w = beginRect.w;
    endRect.h = beginRect.h;
    endRect.o = 1;

    if(config.aspectConversion != ASPECT_CONVERSION_PAD_BLACK)
    {
        double destDar = producer->profile()->dar();
        double sourceW = producer->get_double("meta.media.width");
        double sourceH = producer->get_double("meta.media.height");
        double sourceAr = producer->get_double("aspect_ratio");
        double sourceDar = destDar;
        if( sourceW && sourceH && sourceAr )
        {
            sourceDar = sourceW * sourceAr / sourceH;
        }

        if(sourceDar > destDar)
        {
            // Crop sides to fit height
            beginRect.w = (double)producer->profile()->width() * sourceDar / destDar;
            beginRect.h = producer->profile()->height();
            beginRect.y = 0;
            endRect.w = beginRect.w;
            endRect.h = beginRect.h;
            endRect.y = beginRect.y;
            if(config.aspectConversion == ASPECT_CONVERSION_CROP_CENTER)
            {
                beginRect.x = ((double)producer->profile()->width() - beginRect.w) / 2.0;
                endRect.x = beginRect.x;
            }
            else
            {
                beginRect.x = 0;
                endRect.x = (double)producer->profile()->width() - endRect.w;
            }
        }
        else if(destDar > sourceDar)
        {
            // Crop top and bottom to fit width.
            beginRect.w = producer->profile()->width();
            beginRect.h = (double)producer->profile()->height() * destDar / sourceDar;
            beginRect.x = 0;
            endRect.w = beginRect.w;
            endRect.h = beginRect.h;
            endRect.x = beginRect.x;
            if(config.aspectConversion == ASPECT_CONVERSION_CROP_CENTER)
            {
                beginRect.y = ((double)producer->profile()->height() - beginRect.h) / 2.0;
                endRect.y = beginRect.y;
            }
            else
            {
                beginRect.y = 0;
                endRect.y =  (double)producer->profile()->height() - endRect.h;;
            }
        }
    }

    if (config.zoomPercent != 0)
    {
        double endScale = (double)config.zoomPercent / 100.0;
        endRect.x = endRect.x - (endScale * endRect.w / 2.0);
        endRect.y = endRect.y - (endScale * endRect.h / 2.0);
        endRect.w = endRect.w + (endScale * endRect.w);
        endRect.h = endRect.h + (endScale * endRect.h);;
    }

    filter->anim_set( "transition.rect", beginRect, 0);
    filter->anim_set( "transition.rect", endRect, endPosition);
    filter->set("transition.fill", 1);
    filter->set("transition.distort", 0);
    filter->set("transition.valign", "middle");
    filter->set("transition.halign", "center");
    filter->set("transition.threads", 0);
    filter->set("background", "color:#000000");
    filter->set("shotcut:filter", "affineSizePosition");
    filter->set("shotcut:animIn", producer->frames_to_time(endPosition, mlt_time_clock));
    filter->set("shotcut:animOut", producer->frames_to_time(0, mlt_time_clock));
}

void SlideshowGeneratorWidget::applyLumaTransitionProperties(Mlt::Transition* luma, SlideshowConfig& config)
{
    int index = config.transitionStyle;

    if (index == 0) {
        // Random: pick any number other than 0
        index = rand() % 24 + 1;
    }

    if (index == 1) {
        // Dissolve
        luma->set("resource", "");
    } else {
        luma->set("resource", QString("%luma%1.pgm").arg(index - 1, 2, 10, QChar('0')).toLatin1().constData());
    }
    luma->set("softness", config.transitionSoftness / 100.0);
    luma->set("progressive", 1);
    if (!Settings.playerGPU()) luma->set("alpha_over", 1);
}

void SlideshowGeneratorWidget::on_parameterChanged()
{
    if (m_transitionDurationSpinner->value() > m_clipDurationSpinner->value() / 2 )
    {
        m_transitionDurationSpinner->setValue(m_clipDurationSpinner->value() / 2);
    }
    if (m_transitionDurationSpinner->value() == 0 )
    {
        m_transitionStyleCombo->setEnabled(false);
        m_softnessSpinner->setEnabled(false);
    }
    else
    {
        m_transitionStyleCombo->setEnabled(true);
        m_softnessSpinner->setEnabled(true);
    }

    m_preview->stop();
    m_preview->showText(tr("Generating Preview..."));
    m_mutex.lock();
    m_refreshPreview = true;
    m_config.clipDuration = m_clipDurationSpinner->value();
    m_config.aspectConversion = m_aspectConversionCombo->currentIndex();
    m_config.zoomPercent = m_zoomPercentSpinner->value();
    m_config.transitionDuration = m_transitionDurationSpinner->value();
    m_config.transitionStyle = m_transitionStyleCombo->currentIndex();
    m_config.transitionSoftness = m_softnessSpinner->value();
    if(m_future.isFinished() || m_future.isCanceled())
    {
        // Generate the preview producer in another thread because it can take some time
        m_future = QtConcurrent::run(this, &SlideshowGeneratorWidget::generatePreviewSlideshow);
    }
    m_mutex.unlock();
}

void SlideshowGeneratorWidget::generatePreviewSlideshow()
{
    m_mutex.lock();
    while(m_refreshPreview)
    {
        m_refreshPreview = false;

        m_mutex.unlock();
        Mlt::Producer* newProducer = getSlideshow();
        m_mutex.lock();

        if(!m_refreshPreview)
        {
            if(m_previewProducer)
            {
                delete m_previewProducer;
            }
            m_previewProducer = newProducer;
            QMetaObject::invokeMethod(this, "startPreview", Qt::QueuedConnection);
        }
        else
        {
            // Another refresh was requested while we generated this producer.
            // Delete it and make a new one.
            delete newProducer;
        }
    }
    m_mutex.unlock();
}

void SlideshowGeneratorWidget::startPreview()
{
    m_mutex.lock();
    if(m_previewProducer)
    {
        m_preview->start(m_previewProducer);
    }
    m_previewProducer = nullptr;
    m_mutex.unlock();
}
