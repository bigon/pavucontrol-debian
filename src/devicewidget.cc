/***
  This file is part of pavucontrol.

  Copyright 2006-2008 Lennart Poettering
  Copyright 2009 Colin Guthrie

  pavucontrol is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  pavucontrol is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with pavucontrol. If not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "devicewidget.h"
#include "channelwidget.h"

/*** DeviceWidget ***/
DeviceWidget::DeviceWidget(BaseObjectType* cobject, const Glib::RefPtr<Gnome::Glade::Xml>& x) :
    MinimalStreamWidget(cobject, x)  {

    x->get_widget("lockToggleButton", lockToggleButton);
    x->get_widget("muteToggleButton", muteToggleButton);
    x->get_widget("defaultToggleButton", defaultToggleButton);
    x->get_widget("portSelect", portSelect);
    x->get_widget("portList", portList);

    muteToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onMuteToggleButton));
    defaultToggleButton->signal_clicked().connect(sigc::mem_fun(*this, &DeviceWidget::onDefaultToggleButton));

    treeModel = Gtk::ListStore::create(portModel);
    portList->set_model(treeModel);
    portList->pack_start(portModel.desc);

    portList->signal_changed().connect(sigc::mem_fun(*this, &DeviceWidget::onPortChange));

    for (unsigned i = 0; i < PA_CHANNELS_MAX; i++)
        channelWidgets[i] = NULL;
}

void DeviceWidget::setChannelMap(const pa_channel_map &m, bool can_decibel) {
    channelMap = m;

    for (int i = 0; i < m.channels; i++) {
        ChannelWidget *cw = channelWidgets[i] = ChannelWidget::create();
        cw->channel = i;
        cw->can_decibel = can_decibel;
        cw->minimalStreamWidget = this;
        char text[64];
        snprintf(text, sizeof(text), "<b>%s</b>", pa_channel_position_to_pretty_string(m.map[i]));
        cw->channelLabel->set_markup(text);
        channelsVBox->pack_start(*cw, false, false, 0);
    }

    lockToggleButton->set_sensitive(m.channels > 1);
}

void DeviceWidget::setVolume(const pa_cvolume &v, bool force) {
    g_assert(v.channels == channelMap.channels);

    volume = v;

    if (timeoutConnection.empty() || force) { /* do not update the volume when a volume change is still in flux */
        for (int i = 0; i < volume.channels; i++)
            channelWidgets[i]->setVolume(volume.values[i]);
    }
}

void DeviceWidget::updateChannelVolume(int channel, pa_volume_t v) {
    pa_cvolume n;
    g_assert(channel < volume.channels);

    n = volume;
    if (lockToggleButton->get_active())
        pa_cvolume_set(&n, n.channels, v);
    else
        n.values[channel] = v;

    setVolume(n, true);

    if (timeoutConnection.empty())
        timeoutConnection = Glib::signal_timeout().connect(sigc::mem_fun(*this, &DeviceWidget::timeoutEvent), 100);
}

void DeviceWidget::onMuteToggleButton() {

    lockToggleButton->set_sensitive(!muteToggleButton->get_active());

    for (int i = 0; i < channelMap.channels; i++)
        channelWidgets[i]->set_sensitive(!muteToggleButton->get_active());
}

void DeviceWidget::onDefaultToggleButton() {
}

void DeviceWidget::setDefault(bool isDefault) {
    defaultToggleButton->set_active(isDefault);
    /*defaultToggleButton->set_sensitive(!isDefault);*/
}

bool DeviceWidget::timeoutEvent() {
    executeVolumeUpdate();
    return false;
}

void DeviceWidget::executeVolumeUpdate() {
}

void DeviceWidget::setBaseVolume(pa_volume_t v) {

    if (channelMap.channels > 0)
        channelWidgets[channelMap.channels-1]->setBaseVolume(v);
}

void DeviceWidget::setSteps(unsigned n) {

    for (int i = 0; i < channelMap.channels; i++)
        channelWidgets[channelMap.channels-1]->setSteps(n);
}

void DeviceWidget::prepareMenu() {
    int idx = 0;
    int active_idx = -1;

    treeModel->clear();
    /* Fill the ComboBox's Tree Model */
    for (uint32_t i = 0; i < ports.size(); ++i) {
        Gtk::TreeModel::Row row = *(treeModel->append());
        row[portModel.name] = ports[i].first;
        row[portModel.desc] = ports[i].second;
        if (ports[i].first == activePort)
            active_idx = idx;
        idx++;
    }

    if (active_idx >= 0)
        portList->set_active(active_idx);

    if (ports.size() > 0)
        portSelect->show();
    else
        portSelect->hide();
}
