/** @file vrsettingsdialog.cpp  Settings for virtual reality.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/dialogs/vrsettingsdialog.h"
#include "ui/widgets/cvarsliderwidget.h"
#include "ui/widgets/cvartogglewidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "render/vr.h"
#include "clientapp.h"
#include "api_console.h"
#include <doomsday/console/exec.h>

#include <de/VariableSliderWidget>
#include <de/SignalAction>
#include "CommandAction"

using namespace de;
using namespace ui;

DENG_GUI_PIMPL(VRSettingsDialog)
{
    CVarChoiceWidget *mode;
    CVarToggleWidget *swapEyes;
    CVarSliderWidget *dominantEye;
    CVarSliderWidget *humanHeight;
    CVarSliderWidget *ipd;
    VariableSliderWidget *riftDensity;
    CVarSliderWidget *riftSamples;
    ButtonWidget *riftReset;
    ButtonWidget *riftSetup;
    ButtonWidget *desktopSetup;

    Instance(Public *i)
        : Base(i)
        , riftReset(0)
        , riftSetup(0)
    {
        ScrollAreaWidget &area = self.area();

        area.add(mode = new CVarChoiceWidget("rend-vr-mode"));
        mode->items()
                << new ChoiceItem(tr("No stereo"),                VRConfig::Mono)
                << new ChoiceItem(tr("Anaglyph (green/magenta)"), VRConfig::GreenMagenta)
                << new ChoiceItem(tr("Anaglyph (red/cyan)"),      VRConfig::RedCyan)
                << new ChoiceItem(tr("Left eye only"),            VRConfig::LeftOnly)
                << new ChoiceItem(tr("Right eye only"),           VRConfig::RightOnly)
                << new ChoiceItem(tr("Top/bottom"),               VRConfig::TopBottom)
                << new ChoiceItem(tr("Side-by-side"),             VRConfig::SideBySide)
                << new ChoiceItem(tr("Parallel"),                 VRConfig::Parallel)
                << new ChoiceItem(tr("Cross-eye"),                VRConfig::CrossEye)
                << new ChoiceItem(tr("Oculus Rift"),              VRConfig::OculusRift)
                << new ChoiceItem(tr("Hardware stereo"),          VRConfig::QuadBuffered);

        area.add(swapEyes    = new CVarToggleWidget("rend-vr-swap-eyes", tr("Swap Eyes")));
        area.add(dominantEye = new CVarSliderWidget("rend-vr-dominant-eye"));
        area.add(humanHeight = new CVarSliderWidget("rend-vr-player-height"));
        area.add(riftSamples = new CVarSliderWidget("rend-vr-rift-samples"));

        area.add(ipd = new CVarSliderWidget("rend-vr-ipd"));
        ipd->setDisplayFactor(1000);
        ipd->setPrecision(1);

        if(vrCfg().oculusRift().isReady())
        {
            area.add(riftDensity = new VariableSliderWidget(App::config("vr.oculusRift.pixelDensity"),
                     Ranged(0.5, 1.0), .01));
            riftDensity->setPrecision(2);

            area.add(riftReset = new ButtonWidget);
            riftReset->setText(tr("Recenter Tracking"));
            riftReset->setAction(new CommandAction("resetriftpose"));

            area.add(riftSetup = new ButtonWidget);
            riftSetup->setText(tr("Apply Rift Settings"));
            riftSetup->setAction(new SignalAction(thisPublic, SLOT(autoConfigForOculusRift())));
        }

        area.add(desktopSetup = new ButtonWidget);
        desktopSetup->setText(tr("Apply Desktop Settings"));
        desktopSetup->setAction(new SignalAction(thisPublic, SLOT(autoConfigForDesktop())));
    }

    void fetch()
    {
        foreach(Widget *child, self.area().childWidgets())
        {
            if(ICVarWidget *w = child->maybeAs<ICVarWidget>())
            {
                w->updateFromCVar();
            }
        }
    }
};

VRSettingsDialog::VRSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    heading().setText(tr("3D & VR Settings"));

    LabelWidget *modeLabel     = LabelWidget::newWithText(tr("Stereo Mode:"), &area());
    LabelWidget *heightLabel   = LabelWidget::newWithText(tr("Height (m):"), &area());
    LabelWidget *ipdLabel      = LabelWidget::newWithText(tr("IPD (mm):"), &area());
    LabelWidget *dominantLabel = LabelWidget::newWithText(tr("Dominant Eye:"), &area());

    // Layout.
    GridLayout layout(area().contentRule().left(), area().contentRule().top());
    layout.setGridSize(2, 0);
    layout.setColumnAlignment(0, ui::AlignRight);

    layout << *modeLabel     << *d->mode
           << *heightLabel   << *d->humanHeight
           << *ipdLabel      << *d->ipd
           << *dominantLabel << *d->dominantEye
           << Const(0)       << *d->swapEyes;

    LabelWidget *ovrLabel    = LabelWidget::newWithText(_E(D) + tr("Oculus Rift"), &area());
    LabelWidget *sampleLabel = LabelWidget::newWithText(tr("Multisampling:"), &area());
    ovrLabel->setFont("separator.label");
    ovrLabel->margins().setTop("gap");
    sampleLabel->setTextLineAlignment(ui::AlignRight);

    layout.setCellAlignment(Vector2i(0, 5), ui::AlignLeft);
    layout.append(*ovrLabel, 2);

    LabelWidget *utilLabel = LabelWidget::newWithText(tr("Utilities:"), &area());
    if(vrCfg().oculusRift().isReady())
    {
        layout << *sampleLabel << *d->riftSamples
               << *LabelWidget::newWithText(tr("Pixel Density:"), &area())
               << *d->riftDensity;

        layout << *utilLabel << *d->riftReset
               << Const(0)   << *d->riftSetup
               << Const(0)   << *d->desktopSetup;
    }
    else
    {
        layout << *utilLabel << *d->desktopSetup;
    }

    area().setContentSize(layout.width(), layout.height());

    buttons()
            << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    d->fetch();
}

void VRSettingsDialog::resetToDefaults()
{
    Con_SetInteger("rend-vr-mode",          VRConfig::Mono);
    Con_SetInteger("rend-vr-swap-eyes",     0);
    Con_SetFloat  ("rend-vr-dominant-eye",  0);
    Con_SetFloat  ("rend-vr-player-height", 1.75f);
    Con_SetFloat  ("rend-vr-ipd",           0.064f);
    Con_SetFloat  ("rend-vr-rift-latency",  0.030f);
    Con_SetInteger("rend-vr-rift-samples",  2);

    d->fetch();
}

void VRSettingsDialog::autoConfigForOculusRift()
{
    //Con_Execute(CMDS_DDAY, "setfullres 1280 800", false, false);

    Con_Execute(CMDS_DDAY, "bindcontrol lookpitch head-pitch", false, false);
    Con_Execute(CMDS_DDAY, "bindcontrol yawbody head-yaw", false, false);

    /// @todo This would be a good use case for cvar overriding. -jk

    Con_SetInteger("rend-vr-mode", VRConfig::OculusRift);
    App::config().set("window.main.fsaa", false);
    Con_SetFloat  ("vid-gamma", 1.176f);
    Con_SetFloat  ("vid-contrast", 1.186f);
    Con_SetFloat  ("vid-bright", .034f);
    Con_SetFloat  ("view-bob-height", .2f);
    Con_SetFloat  ("msg-scale", 1);
    Con_SetFloat  ("hud-scale", 1);

    d->fetch();

    ClientApp::vr().oculusRift().moveWindowToScreen(OculusRift::HMDScreen);
}

void VRSettingsDialog::autoConfigForDesktop()
{
    Con_SetInteger("rend-vr-mode", VRConfig::Mono);
    Con_SetFloat  ("vid-gamma", 1);
    Con_SetFloat  ("vid-contrast", 1);
    Con_SetFloat  ("vid-bright", 0);
    Con_SetFloat  ("view-bob-height", 1);
    Con_SetFloat  ("msg-scale", .8f);
    Con_SetFloat  ("hud-scale", .6f);

    d->fetch();

    ClientApp::vr().oculusRift().moveWindowToScreen(OculusRift::DefaultScreen);
}
