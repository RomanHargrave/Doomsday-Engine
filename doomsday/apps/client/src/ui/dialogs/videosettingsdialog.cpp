/** @file videosettingsdialog.cpp  Dialog for video settings.
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

#include "ui/dialogs/videosettingsdialog.h"
#include "ui/widgets/taskbarwidget.h"
#include "ui/widgets/cvarchoicewidget.h"
#include "ui/clientwindow.h"
#include "CommandAction"
#include "clientapp.h"
#include "dd_main.h"

#include <doomsday/console/exec.h>
#include <de/VariableToggleWidget>
#include <de/VariableSliderWidget>
#include <de/ChoiceWidget>
#include <de/SequentialLayout>
#include <de/GridLayout>
#include <de/SignalAction>
#include <de/DisplayMode>
#include <QPoint>

using namespace de;
using namespace ui;

#ifndef MACOSX
#  define USE_COLOR_DEPTH_CHOICE
#endif

DENG2_PIMPL(VideoSettingsDialog),
DENG2_OBSERVES(PersistentCanvasWindow, AttributeChange)
{
    ClientWindow &win;
    VariableToggleWidget *showFps;
    ToggleWidget *fullscreen;
    ToggleWidget *maximized;
    ToggleWidget *centered;
    VariableToggleWidget *fsaa;
    VariableToggleWidget *vsync;
    ChoiceWidget *modes;
    ButtonWidget *windowButton;
#ifdef USE_COLOR_DEPTH_CHOICE
    ChoiceWidget *depths;
#endif
    ListData stretchChoices;
    CVarChoiceWidget *finaleAspect = nullptr;
    CVarChoiceWidget *hudAspect    = nullptr;
    CVarChoiceWidget *inludeAspect = nullptr;
    CVarChoiceWidget *menuAspect   = nullptr;

    Instance(Public *i)
        : Base(i)
        , win(ClientWindow::main())
    {
        ScrollAreaWidget &area = self.area();

        area.add(showFps      = new VariableToggleWidget(App::config("window.main.showFps")));
        area.add(fullscreen   = new ToggleWidget);
        area.add(maximized    = new ToggleWidget);
        area.add(centered     = new ToggleWidget);
        area.add(fsaa         = new VariableToggleWidget(App::config("window.main.fsaa")));
        area.add(vsync        = new VariableToggleWidget(App::config("window.main.vsync")));
        area.add(modes        = new ChoiceWidget);
        area.add(windowButton = new ButtonWidget);
#ifdef USE_COLOR_DEPTH_CHOICE
        area.add(depths       = new ChoiceWidget);
#endif
        win.audienceForAttributeChange() += this;

        if(App_GameLoaded())
        {
            stretchChoices
                << new ChoiceItem(tr("Smart"),        SCALEMODE_SMART_STRETCH)
                << new ChoiceItem(tr("Original 1:1"), SCALEMODE_NO_STRETCH)
                << new ChoiceItem(tr("Stretched"),    SCALEMODE_STRETCH);

            area.add(finaleAspect = new CVarChoiceWidget("rend-finale-stretch"));
            area.add(hudAspect    = new CVarChoiceWidget("rend-hud-stretch"));
            area.add(inludeAspect = new CVarChoiceWidget("inlude-stretch"));
            area.add(menuAspect   = new CVarChoiceWidget("menu-stretch"));

            finaleAspect->setItems(stretchChoices);
            hudAspect->setItems(stretchChoices);
            inludeAspect->setItems(stretchChoices);
            menuAspect->setItems(stretchChoices);
        }
    }

    ~Instance()
    {
        // The common stretchChoices is being deleted now, before the widget tree.
        if(finaleAspect)
        {
            finaleAspect->useDefaultItems();
            hudAspect->useDefaultItems();
            inludeAspect->useDefaultItems();
            menuAspect->useDefaultItems();
        }
        
        win.audienceForAttributeChange() -= this;
    }

    /**
     * Updates the widgets with the actual current state.
     */
    void fetch()
    {
        fullscreen->setActive(win.isFullScreen());
        maximized->setActive(win.isMaximized());
        centered->setActive(win.isCentered());

        windowButton->enable(!win.isFullScreen() && !win.isMaximized());

        // Select the current resolution/size in the mode list.
        Canvas::Size current = win.fullscreenSize();

        // Update selected display mode.
        ui::Data::Pos closest = ui::Data::InvalidPos;
        int delta = 0;
        for(ui::Data::Pos i = 0; i < modes->items().size(); ++i)
        {
            QPoint const res = modes->items().at(i).data().toPoint();
            int dx = res.x() - current.x;
            int dy = res.y() - current.y;
            int d = dx*dx + dy*dy;
            if(closest == ui::Data::InvalidPos || d < delta)
            {
                closest = i;
                delta = d;
            }
        }
        modes->setSelected(closest);

#ifdef USE_COLOR_DEPTH_CHOICE
        // Select the current color depth in the depth list.
        depths->setSelected(depths->items().findData(win.colorDepthBits()));
#endif

        foreach(Widget *child, self.area().childWidgets())
        {
            if(ICVarWidget *cw = child->maybeAs<ICVarWidget>())
                cw->updateFromCVar();
        }
    }

    void windowAttributesChanged(PersistentCanvasWindow &)
    {
        fetch();
    }
};

VideoSettingsDialog::VideoSettingsDialog(String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this))
{
    bool const gotDisplayMode = DisplayMode_Count() > 0;

    heading().setText(tr("Video Settings"));

    // Toggles for video/window options.
    d->fullscreen->setText(tr("Fullscreen"));
    d->fullscreen->setAction(new CommandAction("togglefullscreen"));

    d->maximized->setText(tr("Maximized"));
    d->maximized->setAction(new CommandAction("togglemaximized"));

    d->centered->setText(tr("Center Window"));
    d->centered->setAction(new CommandAction("togglecentered"));

    d->showFps->setText(tr("Show FPS"));

    d->fsaa->setText(tr("Antialias"));

    d->vsync->setText(tr("VSync"));

#ifdef USE_COLOR_DEPTH_CHOICE
    LabelWidget *colorLabel = 0;
#endif
    if(gotDisplayMode)
    {
        // Choice of display modes + 16/32-bit color depth.
        d->modes->setOpeningDirection(ui::Up);
        if(DisplayMode_Count() > 10)
        {
            d->modes->popup().menu().setGridSize(2, ui::Expand, 0, ui::Expand);
        }
        for(int i = 0; i < DisplayMode_Count(); ++i)
        {
            DisplayMode const *m = DisplayMode_ByIndex(i);
            QPoint const res(m->width, m->height);

            if(d->modes->items().findData(res) != ui::Data::InvalidPos)
            {
                // Got this already.
                continue;
            }

            String desc = String("%1 x %2 (%3:%4)")
                    .arg(m->width).arg(m->height)
                    .arg(m->ratioX).arg(m->ratioY);

            d->modes->items() << new ChoiceItem(desc, res);
        }

#ifdef USE_COLOR_DEPTH_CHOICE
        colorLabel = new LabelWidget;
        colorLabel->setText(tr("Colors:"));
        area().add(colorLabel);

        d->depths->items()
                << new ChoiceItem(tr("32-bit"), 32)
                << new ChoiceItem(tr("24-bit"), 24)
                << new ChoiceItem(tr("16-bit"), 16);
#endif
    }

    buttons()
            << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Close"))
            << new DialogButtonItem(DialogWidget::Action, tr("Reset to Defaults"),
                                    new SignalAction(this, SLOT(resetToDefaults())));

    d->windowButton->setImage(style().images().image("window.icon"));
    d->windowButton->setOverrideImageSize(style().fonts().font("default").height().valuei());
    d->windowButton->setAction(new SignalAction(this, SLOT(showWindowMenu())));

    // Layout all widgets.
    Rule const &gap = style().rules().rule("dialog.gap");

    GridLayout layout(area().contentRule().left(),
                      area().contentRule().top(), GridLayout::RowFirst);
    layout.setGridSize(2, 3);
    layout.setColumnPadding(style().rules().rule("unit"));
    layout << *d->showFps
           << *d->fsaa
           << *d->vsync
           << *d->fullscreen
           << *d->maximized
           << *d->centered;

    GridLayout modeLayout(d->vsync->rule().left(), d->vsync->rule().bottom() + gap);
    modeLayout.setGridSize(2, 0);
    modeLayout.setColumnAlignment(0, ui::AlignRight);

    if(gotDisplayMode)
    {
        modeLayout << *LabelWidget::newWithText(tr("Resolution:"), &area());

        modeLayout.append(*d->modes, d->modes->rule().width() + d->windowButton->rule().width());

        d->windowButton->rule()
                .setInput(Rule::Top,  d->modes->rule().top())
                .setInput(Rule::Left, d->modes->rule().right());

#ifdef USE_COLOR_DEPTH_CHOICE
        modeLayout << *colorLabel << *d->depths;
#endif

        auto *adjustButton = new ButtonWidget;
        adjustButton->setText(tr("Color Adjustments..."));
        adjustButton->setAction(new SignalAction(this, SLOT(showColorAdjustments())));
        area().add(adjustButton);

        modeLayout << Const(0) << *adjustButton;
    }

    if(d->inludeAspect)
    {
        // Aspect ratio options.
        auto *aspectLabel = LabelWidget::newWithText(_E(D) + tr("Aspect Ratios"), &area());
        aspectLabel->setFont("separator.label");
        aspectLabel->margins().setTop("gap");
        modeLayout.setCellAlignment(Vector2i(0, modeLayout.gridSize().y), ui::AlignLeft);
        modeLayout.append(*aspectLabel, 2)
                << *LabelWidget::newWithText(tr("Player Weapons:"), &area()) << *d->hudAspect
                << *LabelWidget::newWithText(tr("Intermissions:"), &area()) << *d->inludeAspect
                << *LabelWidget::newWithText(tr("Finales:"), &area()) << *d->finaleAspect
                << *LabelWidget::newWithText(tr("Menus:"), &area()) << *d->menuAspect;
    }

    area().setContentSize(OperatorRule::maximum(layout.width(), modeLayout.width()),
                          layout.height() + gap + modeLayout.height());

    d->fetch();

    connect(d->modes,  SIGNAL(selectionChangedByUser(uint)), this, SLOT(changeMode(uint)));

#ifdef USE_COLOR_DEPTH_CHOICE
    connect(d->depths, SIGNAL(selectionChangedByUser(uint)), this, SLOT(changeColorDepth(uint)));
#endif
}

void VideoSettingsDialog::resetToDefaults()
{
    ClientApp::windowSystem().settings().resetToDefaults();

    d->fetch();
}

void VideoSettingsDialog::changeMode(uint selected)
{
    QPoint const res = d->modes->items().at(selected).data().toPoint();

    int attribs[] = {
        ClientWindow::FullscreenWidth,  int(res.x()),
        ClientWindow::FullscreenHeight, int(res.y()),
        ClientWindow::End
    };

    d->win.changeAttributes(attribs);
}

void VideoSettingsDialog::changeColorDepth(uint selected)
{
#ifdef USE_COLOR_DEPTH_CHOICE
    Con_Executef(CMDS_DDAY, true, "setcolordepth %i",
                 d->depths->items().at(selected).data().toInt());
#else
    DENG2_UNUSED(selected);
#endif
}

void VideoSettingsDialog::showColorAdjustments()
{
    d->win.showColorAdjustments();
    d->win.taskBar().closeConfigMenu();
}

void VideoSettingsDialog::showWindowMenu()
{
    PopupMenuWidget *menu = new PopupMenuWidget;
    menu->setDeleteAfterDismissed(true);
    add(menu);

    menu->setAnchorAndOpeningDirection(d->windowButton->rule(), ui::Up);
    menu->items()
            << new ActionItem(tr("Apply to Window"),
                              new SignalAction(this, SLOT(applyModeToWindow())));
    menu->open();
}

void VideoSettingsDialog::applyModeToWindow()
{
    QPoint const res = d->modes->selectedItem().data().toPoint();

    int attribs[] = {
        ClientWindow::Width,  res.x(),
        ClientWindow::Height, res.y(),
        ClientWindow::End
    };

    d->win.changeAttributes(attribs);
}
