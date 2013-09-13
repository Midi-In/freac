 /* fre:ac - free audio converter
  * Copyright (C) 2001-2013 Robert Kausch <robert.kausch@freac.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <startgui.h>
#include <joblist.h>
#include <playback.h>
#include <config.h>
#include <utilities.h>
#include <resources.h>

#include <engine/converter.h>

#include <jobs/joblist/addtracks.h>
#include <jobs/other/checkforupdates.h>

#include <dialogs/config/config.h>
#include <dialogs/config/configcomponent.h>

#include <dialogs/adddirectory.h>
#include <dialogs/addpattern.h>

#include <cddb/cddbremote.h>
#include <cddb/cddbcache.h>

#include <dialogs/cddb/query.h>
#include <dialogs/cddb/submit.h>
#include <dialogs/cddb/manage.h>
#include <dialogs/cddb/managequeries.h>
#include <dialogs/cddb/managesubmits.h>

#ifdef __WIN32__
#	include <windows.h>
#	include <dbt.h>

#	include <smooth/init.win32.h>
#endif

using namespace smooth::GUI::Dialogs;
using namespace smooth::Input;

using namespace BoCA;
using namespace BoCA::AS;

Int StartGUI(const Array<String> &args)
{
	BonkEnc::BonkEncGUI	*app = BonkEnc::BonkEncGUI::Get();

	app->Loop();

	BonkEnc::BonkEncGUI::Free();

	return 0;
}

BonkEnc::BonkEncGUI *BonkEnc::BonkEncGUI::Get()
{
	if (instance == NIL) new BonkEncGUI();

	return (BonkEncGUI *) instance;
}

Void BonkEnc::BonkEncGUI::Free()
{
	if (instance != NIL) delete (BonkEncGUI *) instance;
}

BonkEnc::BonkEncGUI::BonkEncGUI()
{
	BoCA::Config	*config	= BoCA::Config::Get();
	BoCA::I18n	*i18n	= BoCA::I18n::Get();

	config->enable_console = False;

	/* Set interface language.
	 */
	String	 language = config->GetStringValue(Config::CategorySettingsID, Config::SettingsLanguageID, Config::SettingsLanguageDefault);

	if (language != NIL) i18n->ActivateLanguage(language);
	else		     i18n->SelectUserDefaultLanguage();

	config->SetStringValue(Config::CategorySettingsID, Config::SettingsLanguageID, i18n->GetActiveLanguageID());

	/* Setup attributes.
	 */
	clicked_configuration = -1;
	clicked_drive	      = -1;
	clicked_encoder	      = -1;

	Rect	 workArea = Screen::GetVirtualScreenMetrics();

	Point	 wndPos = Point(config->GetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosXID, Config::SettingsWindowPosXDefault), config->GetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosYID, Config::SettingsWindowPosYDefault));
	Size	 wndSize = Size(config->GetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeXID, Config::SettingsWindowSizeXDefault), config->GetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeYID, Config::SettingsWindowSizeYDefault));

	if (wndPos.x + wndSize.cx > workArea.right + 2 ||
	    wndPos.y + wndSize.cy > workArea.bottom + 2)
	{
		wndPos.x = (Int) Math::Min(workArea.right - 10 - wndSize.cx, wndPos.x);
		wndPos.y = (Int) Math::Min(workArea.bottom - 10 - wndSize.cy, wndPos.y);
	}

	if (wndPos.x < workArea.left - 2 ||
	    wndPos.y < workArea.top - 2)
	{
		wndPos.x = (Int) Math::Max(workArea.left + 10, wndPos.x);
		wndPos.y = (Int) Math::Max(workArea.top + 10, wndPos.y);

		wndSize.cx = (Int) Math::Min(workArea.right - 20, wndSize.cx);
		wndSize.cy = (Int) Math::Min(workArea.bottom - 20, wndSize.cy);
	}

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosXID, wndPos.x);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosYID, wndPos.y);

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeXID, wndSize.cx);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeYID, wndSize.cy);

	mainWnd			= new Window(String(BonkEnc::appLongName).Append(" ").Append(BonkEnc::version), wndPos, wndSize);
	mainWnd->SetRightToLeft(i18n->IsActiveLanguageRightToLeft());

	mainWnd_titlebar	= new Titlebar();
	mainWnd_menubar		= new Menubar();
	mainWnd_iconbar		= new Menubar();
	mainWnd_statusbar	= new Statusbar(String(BonkEnc::appLongName).Append(" ").Append(BonkEnc::version).Append(" - Copyright (C) 2001-2013 Robert Kausch"));

	menu_file		= new PopupMenu();
	menu_addsubmenu		= new PopupMenu();
	menu_files		= new PopupMenu();
	menu_drives		= new PopupMenu();

	menu_database		= new PopupMenu();
	menu_database_query	= new PopupMenu();

	menu_options		= new PopupMenu();
	menu_configurations	= new PopupMenu();
	menu_seldrive		= new PopupMenu();

	menu_encode		= new PopupMenu();
	menu_encoders		= new PopupMenu();
	menu_encoder_options	= new PopupMenu();

	menu_help		= new PopupMenu();

	hyperlink		= new Hyperlink(String(BonkEnc::website).Replace("http://", NIL).Replace("/", NIL), NIL, BonkEnc::website, Point(91, -22));
	hyperlink->SetOrientation(OR_UPPERRIGHT);
	hyperlink->SetX(hyperlink->GetUnscaledTextWidth() + 4);
	hyperlink->SetIndependent(True);

	tabs_main		= new TabWidget(Point(6, 7), Size(700, 500));

	tab_layer_joblist	= new LayerJoblist();
	tab_layer_joblist->onRequestSkipTrack.Connect(&Converter::SkipTrack, encoder);

#ifndef BUILD_VIDEO_DOWNLOADER
	tabs_main->Add(tab_layer_joblist);
#endif

	InitExtensionComponents();

	for (Int i = 0; i < extensionComponents.Length(); i++)
	{
		Layer	*mainTabLayer	= extensionComponents.GetNth(i)->getMainTabLayer.Emit();
		Layer	*statusBarLayer = extensionComponents.GetNth(i)->getStatusBarLayer.Emit();

		if (mainTabLayer   != NIL) tabs_main->Add(mainTabLayer);
		if (statusBarLayer != NIL) mainWnd_statusbar->Add(statusBarLayer);
	}

#ifdef BUILD_VIDEO_DOWNLOADER
	tabs_main->Add(tab_layer_joblist);
#endif

	tab_layer_threads	= new LayerThreads();

/* ToDo: Add threads layer once it's ready.
 */
	tabs_main->Add(tab_layer_threads);

	joblist			= tab_layer_joblist->GetJoblist();

	Add(mainWnd);

	SetLanguage();

	mainWnd->Add(hyperlink);
	mainWnd->Add(mainWnd_titlebar);
	mainWnd->Add(mainWnd_statusbar);
	mainWnd->Add(mainWnd_menubar);
	mainWnd->Add(mainWnd_iconbar);
	mainWnd->Add(tabs_main);

	mainWnd->SetIcon(ImageLoader::Load(String(Config::Get()->resourcesPath).Append("icons/freac.png")));

#ifdef __WIN32__
	mainWnd->SetIconDirect(LoadImageA(hInstance, MAKEINTRESOURCEA(IDI_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
#endif

	mainWnd->onChangePosition.Connect(&BonkEncGUI::OnChangePosition, this);
	mainWnd->onChangeSize.Connect(&BonkEncGUI::OnChangeSize, this);

	mainWnd->onEvent.Connect(&BonkEncGUI::MessageProc, this);

	if (config->GetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, Config::SettingsShowTipsDefault)) mainWnd->onShow.Connect(&BonkEncGUI::ShowTipOfTheDay, this);

	mainWnd->doClose.Connect(&BonkEncGUI::ExitProc, this);
	mainWnd->SetMinimumSize(Size(600, 400 + (config->GetIntValue(Config::CategorySettingsID, Config::SettingsShowTitleInfoID, Config::SettingsShowTitleInfoDefault) ? 68 : 0)));

	if (config->GetIntValue(Config::CategorySettingsID, Config::SettingsWindowMaximizedID, Config::SettingsWindowMaximizedDefault)) mainWnd->Maximize();

	encoder->onStartEncoding.Connect(&LayerJoblist::OnEncoderStartEncoding, tab_layer_joblist);
	encoder->onFinishEncoding.Connect(&LayerJoblist::OnEncoderFinishEncoding, tab_layer_joblist);

	encoder->onEncodeTrack.Connect(&LayerJoblist::OnEncoderEncodeTrack, tab_layer_joblist);

	encoder->onTrackProgress.Connect(&LayerJoblist::OnEncoderTrackProgress, tab_layer_joblist);
	encoder->onTotalProgress.Connect(&LayerJoblist::OnEncoderTotalProgress, tab_layer_joblist);

	if (config->GetIntValue(Config::CategorySettingsID, Config::SettingsCheckForUpdatesID, Config::SettingsCheckForUpdatesDefault)) (new JobCheckForUpdates(True))->Schedule();
}

BonkEnc::BonkEncGUI::~BonkEncGUI()
{
	FreeExtensionComponents();

	DeleteObject(mainWnd_menubar);
	DeleteObject(mainWnd_iconbar);
	DeleteObject(mainWnd_titlebar);
	DeleteObject(mainWnd_statusbar);
	DeleteObject(mainWnd);

	DeleteObject(tabs_main);

	DeleteObject(tab_layer_joblist);
	DeleteObject(tab_layer_threads);

	DeleteObject(hyperlink);

	DeleteObject(menu_file);
	DeleteObject(menu_addsubmenu);
	DeleteObject(menu_files);
	DeleteObject(menu_drives);

	DeleteObject(menu_database);
	DeleteObject(menu_database_query);

	DeleteObject(menu_options);
	DeleteObject(menu_configurations);
	DeleteObject(menu_seldrive);

	DeleteObject(menu_encode);
	DeleteObject(menu_encoders);
	DeleteObject(menu_encoder_options);

	DeleteObject(menu_help);
}

Void BonkEnc::BonkEncGUI::InitExtensionComponents()
{
	Registry	&boca = Registry::Get();

	for (Int i = 0; i < boca.GetNumberOfComponents(); i++)
	{
		if (boca.GetComponentType(i) != BoCA::COMPONENT_TYPE_EXTENSION) continue;

		ExtensionComponent	*component = (ExtensionComponent *) boca.CreateComponentByID(boca.GetComponentID(i));

		if (component != NIL) extensionComponents.Add(component);
	}
}

Void BonkEnc::BonkEncGUI::FreeExtensionComponents()
{
	Registry	&boca = Registry::Get();

	for (Int i = 0; i < extensionComponents.Length(); i++)
	{
		boca.DeleteComponent(extensionComponents.GetNth(i));
	}

	extensionComponents.RemoveAll();
}

Bool BonkEnc::BonkEncGUI::ExitProc()
{
	BoCA::Config	*config = BoCA::Config::Get();

	/* Check if encoder is running.
	 */
	if (encoder->IsEncoding())
	{
		BoCA::I18n	*i18n = BoCA::I18n::Get();

		i18n->SetContext("Messages");

		if (Message::Button::No == QuickMessage(i18n->TranslateString("The encoding thread is still running! Do you really want to quit?"), i18n->TranslateString("Currently encoding"), Message::Buttons::YesNo, Message::Icon::Question)) return False;

		encoder->Stop();
	}

	/* Stop playback if playing a track
	 */
	Playback::Get()->Stop();

	/* Notify components that we are about to quit.
	 */
	BoCA::Application::Get()->onQuit.Emit();

	/* Save main window position.
	 */
	Rect	 wndRect = mainWnd->GetRestoredWindowRect() - mainWnd->GetSizeModifier();

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosXID, wndRect.left);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosYID, wndRect.top);

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeXID, wndRect.right - wndRect.left);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeYID, wndRect.bottom - wndRect.top);

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowMaximizedID, mainWnd->IsMaximized());

	config->SaveSettings();

	return True;
}

Void BonkEnc::BonkEncGUI::MessageProc(Int message, Int wParam, Int lParam)
{
#ifdef __WIN32__
	BoCA::Config	*config = BoCA::Config::Get();

	switch (message)
	{
		case WM_DEVICECHANGE:
			if (wParam == DBT_DEVICEARRIVAL && config->cdrip_numdrives > 0 && config->GetIntValue(Config::CategoryRipperID, Config::RipperAutoReadContentsID, Config::RipperAutoReadContentsDefault))
			{
				if (((DEV_BROADCAST_HDR *) lParam)->dbch_devicetype != DBT_DEVTYP_VOLUME || !(((DEV_BROADCAST_VOLUME *) lParam)->dbcv_flags & DBTF_MEDIA)) break;

				/* Get drive letter from message.
				 */
				String	 driveLetter = String(" :");

				for (Int drive = 0; drive < 26; drive++)
				{
					if (((DEV_BROADCAST_VOLUME *) lParam)->dbcv_unitmask >> drive & 1)
					{
						driveLetter[0] = drive + 'A';

						break;
					}
				}

				if (driveLetter[0] == ' ') break;

				Int	 trackLength = 0;

				/* Read length of first track using MCI.
				 */
				MCI_OPEN_PARMSA	 openParms;

				openParms.lpstrDeviceType  = (LPSTR) MCI_DEVTYPE_CD_AUDIO;
				openParms.lpstrElementName = driveLetter;

				MCIERROR	 error = mciSendCommandA(NIL, MCI_OPEN, MCI_WAIT | MCI_OPEN_SHAREABLE | MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID | MCI_OPEN_ELEMENT, (DWORD_PTR) &openParms);

				if (error == 0)
				{
					MCI_SET_PARMS		 setParms;

					setParms.dwTimeFormat	= MCI_FORMAT_MSF;

					mciSendCommandA(openParms.wDeviceID, MCI_SET, MCI_WAIT | MCI_SET_TIME_FORMAT, (DWORD_PTR) &setParms);

					MCI_STATUS_PARMS	 statusParms;

					statusParms.dwItem	= MCI_STATUS_LENGTH;
					statusParms.dwTrack	= 1;

					mciSendCommandA(openParms.wDeviceID, MCI_STATUS, MCI_WAIT | MCI_STATUS_ITEM | MCI_TRACK, (DWORD_PTR) &statusParms);

					trackLength = MCI_MSF_MINUTE(statusParms.dwReturn) * 60 * 75 +
						      MCI_MSF_SECOND(statusParms.dwReturn) * 75	     +
						      MCI_MSF_FRAME (statusParms.dwReturn);

					MCI_GENERIC_PARMS	 closeParms;

					mciSendCommandA(openParms.wDeviceID, MCI_CLOSE, MCI_WAIT, (DWORD_PTR) &closeParms);
				}

				/* Look for the actual drive using
				 * the length of the first track.
				 */
				if (trackLength > 0)
				{
					Bool	 ok = False;
					Int	 drive = 0;

					Registry		&boca = Registry::Get();
					DeviceInfoComponent	*info = (DeviceInfoComponent *) boca.CreateComponentByID("cdrip-info");

					if (info != NIL)
					{
						for (drive = 0; drive < info->GetNumberOfDevices(); drive++)
						{
							const MCDI	&mcdi = info->GetNthDeviceMCDI(drive);
							Int		 length = mcdi.GetNthEntryOffset(1) - mcdi.GetNthEntryOffset(0);

							if (mcdi.GetNthEntryType(0) == ENTRY_AUDIO && length == trackLength) { ok = True; break; }
						}

						boca.DeleteComponent(info);
					}

					if (ok)
					{
						Int	 activeDrive = config->GetIntValue(Config::CategoryRipperID, Config::RipperActiveDriveID, Config::RipperActiveDriveDefault);

						config->SetIntValue(Config::CategoryRipperID, Config::RipperActiveDriveID, drive);

						ReadCD(True);

						config->SetIntValue(Config::CategoryRipperID, Config::RipperActiveDriveID, activeDrive);
					}
				}
			}

			break;
	}
#endif
}

Void BonkEnc::BonkEncGUI::OnChangePosition(const Point &nPos)
{
	BoCA::Config	*config = BoCA::Config::Get();

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosXID, mainWnd->GetPosition().x);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowPosYID, mainWnd->GetPosition().y);
}

Void BonkEnc::BonkEncGUI::OnChangeSize(const Size &nSize)
{
	BoCA::Config	*config = BoCA::Config::Get();

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeXID, mainWnd->GetSize().cx);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsWindowSizeYID, mainWnd->GetSize().cy);

	mainWnd->SetStatusText(String(BonkEnc::appLongName).Append(" ").Append(BonkEnc::version).Append(" - Copyright (C) 2001-2013 Robert Kausch"));

	Rect	 clientRect = mainWnd->GetClientRect();
	Size	 clientSize = Size(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

	tabs_main->SetSize(clientSize - Size(12, 15));
}

Void BonkEnc::BonkEncGUI::Close()
{
	mainWnd->Close();
}

Void BonkEnc::BonkEncGUI::About()
{
	BoCA::I18n	*i18n	= BoCA::I18n::Get();

	i18n->SetContext("About");

	QuickMessage(String(BonkEnc::appLongName).Append(" ").Append(BonkEnc::version).Append(" (").Append(BonkEnc::architecture).Append(")\nCopyright (C) 2001-2013 Robert Kausch\n\n").Append(String(i18n->TranslateString("Translated by %1.")).Replace("%1", i18n->GetActiveLanguageAuthor())).Append("\n\n").Append(i18n->TranslateString("This program is being distributed under the terms\nof the GNU General Public License (GPL).")), String(i18n->TranslateString("About %1")).Replace("%1", BonkEnc::appName), Message::Buttons::Ok, (wchar_t *) IDI_ICON);
}

Void BonkEnc::BonkEncGUI::ConfigureEncoder()
{
	if (!currentConfig->CanChangeConfig()) return;

	BoCA::Config	*config	= BoCA::Config::Get();

	Registry	&boca = Registry::Get();
	Component	*component = boca.CreateComponentByID(config->GetStringValue(Config::CategorySettingsID, Config::SettingsEncoderID, Config::SettingsEncoderDefault));

	if (component != NIL)
	{
		ConfigLayer	*layer = component->GetConfigurationLayer();

		if (layer != NIL)
		{
			ConfigComponentDialog	*dlg = new ConfigComponentDialog(layer);

			dlg->SetParentWindow(mainWnd);
			dlg->ShowDialog();

			DeleteObject(dlg);
		}
		else
		{
			BoCA::Utilities::ErrorMessage("No configuration dialog available for:\n\n%1", component->GetName());
		}

		boca.DeleteComponent(component);
	}
}

Void BonkEnc::BonkEncGUI::ConfigureSettings()
{
	if (!currentConfig->CanChangeConfig()) return;

	BoCA::Config	*config = BoCA::Config::Get();
	BoCA::I18n	*i18n	= BoCA::I18n::Get();

	ConfigDialog	*dialog	= new ConfigDialog();

	dialog->ShowDialog();

	DeleteObject(dialog);

	for (Int i = 0; i < config->GetNOfConfigurations(); i++)
	{
		if (config->GetNthConfigurationName(i) == config->GetConfigurationName()) clicked_configuration = i;
	}

	if (i18n->GetActiveLanguageID() != config->GetStringValue(Config::CategorySettingsID,
								  Config::SettingsLanguageID,
								  Config::SettingsLanguageDefault)) SetLanguage();

	BoCA::Settings::Get()->onChangeConfigurationSettings.Emit();

	tab_layer_joblist->UpdateEncoderText();
	tab_layer_joblist->UpdateOutputDir();

	CheckBox::internalCheckValues.Emit();
	ToggleUseInputDirectory();

	config->SaveSettings();
}

Void BonkEnc::BonkEncGUI::OnSelectConfiguration()
{
	if (!currentConfig->CanChangeConfig()) return;

	BoCA::Config	*config = BoCA::Config::Get();
	BoCA::I18n	*i18n	= BoCA::I18n::Get();

	config->SetActiveConfiguration(config->GetNthConfigurationName(clicked_configuration));

	if (i18n->GetActiveLanguageID() != config->GetStringValue(Config::CategorySettingsID,
								  Config::SettingsLanguageID,
								  Config::SettingsLanguageDefault)) SetLanguage();

	BoCA::Settings::Get()->onChangeConfigurationSettings.Emit();

	tab_layer_joblist->UpdateEncoderText();
	tab_layer_joblist->UpdateOutputDir();

	CheckBox::internalCheckValues.Emit();
	ToggleUseInputDirectory();

	config->SaveSettings();
}

Void BonkEnc::BonkEncGUI::ReadCD(Bool autoCDRead)
{
	if (!joblist->CanModifyJobList()) return;

	BoCA::Config		*config = BoCA::Config::Get();

	if (clicked_drive >= 0)
	{
		config->SetIntValue(Config::CategoryRipperID, Config::RipperActiveDriveID, clicked_drive);

		clicked_drive = -1;
	}

	Registry		&boca = Registry::Get();
	DeviceInfoComponent	*info = (DeviceInfoComponent *) boca.CreateComponentByID("cdrip-info");

	if (info != NIL)
	{
		const Array<String>	&urls = info->GetNthDeviceTrackList(config->GetIntValue(Config::CategoryRipperID, Config::RipperActiveDriveID, Config::RipperActiveDriveDefault));

		Job	*job = new JobAddTracks(urls, autoCDRead);

		job->Schedule();

		if (config->GetIntValue(Config::CategoryRipperID, Config::RipperAutoRipID, Config::RipperAutoRipDefault)) job->onFinish.Connect(&BonkEncGUI::Encode, this);

		boca.DeleteComponent(info);
	}
}

Void BonkEnc::BonkEncGUI::QueryCDDB()
{
	BoCA::Config	*config = BoCA::Config::Get();

	/* Check if CDDB support is enabled.
	 */
	if (!config->GetIntValue(Config::CategoryFreedbID, Config::FreedbEnableLocalID, Config::FreedbEnableLocalDefault) && !config->GetIntValue(Config::CategoryFreedbID, Config::FreedbEnableRemoteID, Config::FreedbEnableRemoteDefault))
	{
		BoCA::Utilities::ErrorMessage("CDDB support is disabled! Please enable local or\nremote CDDB support in the configuration dialog.");

		return;
	}

	Array<Int>	 discIDs;
	Array<String>	 queryStrings;

	for (Int i = 0; i < joblist->GetNOfTracks(); i++)
	{
		const Track	&track = joblist->GetNthTrack(i);
		const Info	&info = track.GetInfo();

		if (info.mcdi.GetData().Size() > 0)
		{
			Int	 discID	     = CDDB::DiscIDFromMCDI(info.mcdi);
			String	 queryString = CDDB::QueryStringFromMCDI(info.mcdi);

			discIDs.Add(discID, queryString.ComputeCRC32());
			queryStrings.Add(queryString, queryString.ComputeCRC32());
		}
		else if (info.offsets != NIL)
		{
			Int	 discID	     = CDDB::DiscIDFromOffsets(info.offsets);
			String	 queryString = CDDB::QueryStringFromOffsets(info.offsets);

			discIDs.Add(discID, queryString.ComputeCRC32());
			queryStrings.Add(queryString, queryString.ComputeCRC32());
		}
	}

	for (Int j = 0; j < queryStrings.Length(); j++)
	{
		Int		 discID	     = discIDs.GetNth(j);
		const String	&queryString = queryStrings.GetNth(j);
		CDDBInfo	 cdInfo;

		if (config->GetIntValue(Config::CategoryFreedbID, Config::FreedbEnableCacheID, Config::FreedbEnableCacheDefault)) cdInfo = CDDBCache::Get()->GetCacheEntry(discID);

		if (cdInfo == NIL)
		{
			cddbQueryDlg	*dlg = new cddbQueryDlg();

			dlg->SetQueryString(queryString);

			cdInfo = dlg->QueryCDDB(True);

			DeleteObject(dlg);

			if (cdInfo != NIL) CDDBCache::Get()->AddCacheEntry(cdInfo);
		}

		if (cdInfo != NIL)
		{
			for (Int k = 0; k < joblist->GetNOfTracks(); k++)
			{
				Track	 track = joblist->GetNthTrack(k);
				Info	 info = track.GetInfo();

				if ((info.mcdi.GetData().Size() > 0 && discID == CDDB::DiscIDFromMCDI(info.mcdi)) ||
				    (info.offsets != NIL && discID == CDDB::DiscIDFromOffsets(info.offsets)))
				{
					Int	 trackNumber = -1;

					if (track.isCDTrack) trackNumber = track.cdTrack;
					else		     trackNumber = info.track;

					if (trackNumber == -1) continue;

					info.artist	= (cdInfo.dArtist == "Various" ? cdInfo.trackArtists.GetNth(trackNumber - 1) : cdInfo.dArtist);
					info.title	= cdInfo.trackTitles.GetNth(trackNumber - 1);
					info.album	= cdInfo.dTitle;
					info.genre	= cdInfo.dGenre;
					info.year	= cdInfo.dYear;
					info.track	= trackNumber;

					track.SetInfo(info);
					track.SetOriginalInfo(info);

					track.outfile	= NIL;

					BoCA::JobList::Get()->onComponentModifyTrack.Emit(track);
				}
			}
		}
	}
}

Void BonkEnc::BonkEncGUI::QueryCDDBLater()
{
	Array<Int>	 drives;

	for (Int i = 0; i < joblist->GetNOfTracks(); i++)
	{
		const Track	&track = joblist->GetNthTrack(i);

		if (track.isCDTrack) drives.Add(track.drive, track.drive);
	}

	if (drives.Length() > 0)
	{
		CDDBBatch	*queries = new CDDBBatch();

		for (Int j = 0; j < drives.Length(); j++)
		{
			Int		 drive = drives.GetNth(j);
			CDDBRemote	 cddb;

			cddb.SetActiveDrive(drive);

			queries->AddQuery(cddb.GetCDDBQueryString());
		}

		delete queries;
	}
}

Void BonkEnc::BonkEncGUI::SubmitCDDBData()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (!config->GetIntValue(Config::CategoryFreedbID, Config::FreedbEnableLocalID, Config::FreedbEnableLocalDefault) && !config->GetIntValue(Config::CategoryFreedbID, Config::FreedbEnableRemoteID, Config::FreedbEnableRemoteDefault))
	{
		BoCA::Utilities::ErrorMessage("CDDB support is disabled! Please enable local or\nremote CDDB support in the configuration dialog.");

		return;
	}

	cddbSubmitDlg	*dlg = new cddbSubmitDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Void BonkEnc::BonkEncGUI::ManageCDDBData()
{
	cddbManageDlg	*dlg = new cddbManageDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Void BonkEnc::BonkEncGUI::ManageCDDBBatchData()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (!config->GetIntValue(Config::CategoryFreedbID, Config::FreedbEnableRemoteID, Config::FreedbEnableRemoteDefault))
	{
		BoCA::Utilities::ErrorMessage("Remote CDDB support is disabled! Please enable\nremote CDDB support in the configuration dialog.");

		return;
	}

	cddbManageSubmitsDlg	*dlg = new cddbManageSubmitsDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Void BonkEnc::BonkEncGUI::ManageCDDBBatchQueries()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (!config->GetIntValue(Config::CategoryFreedbID, Config::FreedbEnableRemoteID, Config::FreedbEnableRemoteDefault))
	{
		BoCA::Utilities::ErrorMessage("Remote CDDB support is disabled! Please enable\nremote CDDB support in the configuration dialog.");

		return;
	}

	cddbManageQueriesDlg	*dlg = new cddbManageQueriesDlg();

	dlg->ShowDialog();

	DeleteObject(dlg);
}

Bool BonkEnc::BonkEncGUI::SetLanguage()
{
	BoCA::Config	*config	= BoCA::Config::Get();
	BoCA::I18n	*i18n	= BoCA::I18n::Get();

	Bool		 prevRTL = i18n->IsActiveLanguageRightToLeft();

	i18n->ActivateLanguage(config->GetStringValue(Config::CategorySettingsID, Config::SettingsLanguageID, Config::SettingsLanguageDefault));

	MessageDlg::SetDefaultRightToLeft(i18n->IsActiveLanguageRightToLeft());

	if (i18n->IsActiveLanguageRightToLeft() != prevRTL)
	{
		mainWnd->SetUpdateRect(Rect(Point(0, 0), mainWnd->GetRealSize()));
		mainWnd->SetRightToLeft(i18n->IsActiveLanguageRightToLeft());
		mainWnd->Paint(SP_PAINT);
	}

	BoCA::Settings::Get()->onChangeLanguageSettings.Emit();

	FillMenus();

	tabs_main->Paint(SP_PAINT);

	hyperlink->Hide();
	hyperlink->Show();

	return true;
}

Void BonkEnc::BonkEncGUI::FillMenus()
{
	BoCA::Config	*config	= BoCA::Config::Get();
	BoCA::I18n	*i18n	= BoCA::I18n::Get();

	Registry	&boca	= Registry::Get();

	mainWnd_menubar->Hide();
	mainWnd_iconbar->Hide();

	menu_file->RemoveAllEntries();
	menu_addsubmenu->RemoveAllEntries();
	menu_files->RemoveAllEntries();
	menu_drives->RemoveAllEntries();

	menu_database->RemoveAllEntries();
	menu_database_query->RemoveAllEntries();

	menu_options->RemoveAllEntries();
	menu_configurations->RemoveAllEntries();
	menu_seldrive->RemoveAllEntries();

	menu_encode->RemoveAllEntries();
	menu_encoders->RemoveAllEntries();
	menu_encoder_options->RemoveAllEntries();

	menu_help->RemoveAllEntries();

	MenuEntry	*entry	       = NIL;
	Config		*bonkEncConfig = Config::Get();

	i18n->SetContext("Menu::File");

	menu_file->AddEntry(i18n->TranslateString("Add"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:21")), menu_addsubmenu);

	entry = menu_file->AddEntry(i18n->TranslateString("Remove"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:24")));
	entry->onAction.Connect(&JobList::RemoveSelectedTrack, joblist);
	entry->SetShortcut(SC_CTRL, 'R', mainWnd);

	if (boca.GetNumberOfComponentsOfType(COMPONENT_TYPE_PLAYLIST) > 1 || (boca.GetNumberOfComponentsOfType(COMPONENT_TYPE_PLAYLIST) > 0 && !boca.ComponentExists("cuesheet-playlist")))
	{
		menu_file->AddEntry();

		menu_file->AddEntry(i18n->TranslateString("Load joblist..."))->onAction.Connect(&JobList::LoadList, joblist);
		menu_file->AddEntry(i18n->TranslateString("Save joblist..."))->onAction.Connect(&JobList::SaveList, joblist);
	}

	menu_file->AddEntry();

	entry = menu_file->AddEntry(i18n->TranslateString("Clear joblist"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:25")));
	entry->onAction.Connect(&JobList::StartJobRemoveAllTracks, joblist);
	entry->SetShortcut(SC_CTRL | SC_SHIFT, 'R', mainWnd);

	menu_file->AddEntry();

	entry = menu_file->AddEntry(i18n->TranslateString("Exit"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:36")));
	entry->onAction.Connect(&BonkEncGUI::Close, this);
	entry->SetShortcut(SC_ALT, Keyboard::KeyF4, mainWnd);

	entry = menu_addsubmenu->AddEntry(String(i18n->TranslateString("Audio file(s)")).Append("..."), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:22")));
	entry->onAction.Connect(&JobList::AddTrackByDialog, joblist);
	entry->SetShortcut(SC_CTRL, 'F', mainWnd);

	if (config->cdrip_numdrives >= 1)
	{
		DeviceInfoComponent	*info = (DeviceInfoComponent *) boca.CreateComponentByID("cdrip-info");

		if (info != NIL)
		{
			for (Int i = 0; i < info->GetNumberOfDevices(); i++)
			{
				menu_drives->AddEntry(info->GetNthDeviceInfo(i).name, NIL, NIL, NIL, &clicked_drive, i)->onAction.Connect(&BonkEncGUI::ReadCD, this);
			}

			boca.DeleteComponent(info);
		}

		entry = menu_addsubmenu->AddEntry(i18n->TranslateString("Audio CD contents"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:23")));
		entry->onAction.Connect(&BonkEncGUI::ReadCD, this);
		entry->SetShortcut(SC_CTRL, 'D', mainWnd);
	}

	menu_files->AddEntry(String(i18n->TranslateString("By pattern")).Append("..."))->onAction.Connect(&BonkEncGUI::AddFilesByPattern, this);
	menu_files->AddEntry(String(i18n->TranslateString("From folder")).Append("..."))->onAction.Connect(&BonkEncGUI::AddFilesFromDirectory, this);

	menu_addsubmenu->AddEntry();
	menu_addsubmenu->AddEntry(i18n->TranslateString("Audio file(s)"), NIL, menu_files);

	if (config->cdrip_numdrives > 1)
	{
		menu_addsubmenu->AddEntry(i18n->TranslateString("Audio CD contents"), NIL, menu_drives);
	}

	i18n->SetContext("Menu::Database");

	entry = menu_database->AddEntry(i18n->TranslateString("Query CDDB database"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:26")));
	entry->onAction.Connect(&BonkEncGUI::QueryCDDB, this);
	entry->SetShortcut(SC_CTRL, 'Q', mainWnd);

	entry = menu_database->AddEntry(i18n->TranslateString("Submit CDDB data..."), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:27")));
	entry->onAction.Connect(&BonkEncGUI::SubmitCDDBData, this);
	entry->SetShortcut(SC_CTRL, 'S', mainWnd);

	menu_database->AddEntry();

	menu_database->AddEntry(i18n->TranslateString("Query CDDB database later"))->onAction.Connect(&BonkEncGUI::QueryCDDBLater, this);

	menu_database->AddEntry();

	menu_database->AddEntry(i18n->TranslateString("Show queued CDDB entries..."))->onAction.Connect(&BonkEncGUI::ManageCDDBBatchData, this);
	menu_database->AddEntry(i18n->TranslateString("Show queued CDDB queries..."))->onAction.Connect(&BonkEncGUI::ManageCDDBBatchQueries, this);

	menu_database->AddEntry();

	menu_database->AddEntry(i18n->TranslateString("Enable CDDB cache"), NIL, NIL, (Bool *) &config->GetPersistentIntValue(Config::CategoryFreedbID, Config::FreedbEnableCacheID, Config::FreedbEnableCacheDefault));
	menu_database->AddEntry(i18n->TranslateString("Manage CDDB cache entries..."))->onAction.Connect(&BonkEncGUI::ManageCDDBData, this);

	menu_database->AddEntry();

	menu_database->AddEntry(i18n->TranslateString("Automatic CDDB queries"), NIL, NIL, (Bool *) &config->GetPersistentIntValue(Config::CategoryFreedbID, Config::FreedbAutoQueryID, Config::FreedbAutoQueryDefault));

	menu_database_query->AddEntry(i18n->TranslateString("Query CDDB database"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:26")))->onAction.Connect(&BonkEncGUI::QueryCDDB, this);
	menu_database_query->AddEntry(i18n->TranslateString("Query CDDB database later"))->onAction.Connect(&BonkEncGUI::QueryCDDBLater, this);

	i18n->SetContext("Menu::Options");

	entry = menu_options->AddEntry(i18n->TranslateString("General settings..."), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:28")));
	entry->onAction.Connect(&BonkEncGUI::ConfigureSettings, this);
	entry->SetShortcut(SC_CTRL | SC_SHIFT, 'C', mainWnd);

	entry = menu_options->AddEntry(i18n->TranslateString("Configure selected encoder..."), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:29")));
	entry->onAction.Connect(&BonkEncGUI::ConfigureEncoder, this);
	entry->SetShortcut(SC_CTRL | SC_SHIFT, 'E', mainWnd);

	if (config->GetNOfConfigurations() > 1)
	{
		for (Int i = 0; i < config->GetNOfConfigurations(); i++)
		{
			menu_configurations->AddEntry(config->GetNthConfigurationName(i) == "default" ? i18n->TranslateString("Default configuration") : config->GetNthConfigurationName(i), NIL, NIL, NIL, &clicked_configuration, i)->onAction.Connect(&BonkEncGUI::OnSelectConfiguration, this);

			if (config->GetNthConfigurationName(i) == config->GetConfigurationName()) clicked_configuration = i;
		}

		menu_options->AddEntry();
		menu_options->AddEntry(i18n->TranslateString("Active configuration"), NIL, menu_configurations);
	}

	if (config->cdrip_numdrives > 1)
	{
		DeviceInfoComponent	*info = (DeviceInfoComponent *) boca.CreateComponentByID("cdrip-info");

		if (info != NIL)
		{
			for (Int i = 0; i < info->GetNumberOfDevices(); i++)
			{
				menu_seldrive->AddEntry(info->GetNthDeviceInfo(i).name, NIL, NIL, NIL, &config->GetPersistentIntValue(Config::CategoryRipperID, Config::RipperActiveDriveID, Config::RipperActiveDriveDefault), i);
			}

			boca.DeleteComponent(info);
		}

		menu_options->AddEntry();
		menu_options->AddEntry(i18n->TranslateString("Active CD-ROM drive"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:30")), menu_seldrive);
	}

	i18n->SetContext("Menu::Encode");

	entry = menu_encode->AddEntry(i18n->TranslateString("Start encoding"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:31")));
	entry->onAction.Connect(&BonkEncGUI::Encode, this);
	entry->SetShortcut(SC_CTRL, 'E', mainWnd);

	menu_encode->AddEntry(i18n->TranslateString("Pause/resume encoding"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:32")))->onAction.Connect(&BonkEncGUI::PauseResumeEncoding, this);
	menu_encode->AddEntry(i18n->TranslateString("Stop encoding"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:33")))->onAction.Connect(&BonkEncGUI::StopEncoding, this);

	for (Int i = 0; i < boca.GetNumberOfComponents(); i++)
	{
		if (boca.GetComponentType(i) != BoCA::COMPONENT_TYPE_ENCODER) continue;

		menu_encoders->AddEntry(boca.GetComponentName(i), NIL, NIL, NIL, &clicked_encoder, i)->onAction.Connect(&BonkEncGUI::Encode, this);
	}

	if (boca.GetNumberOfComponentsOfType(COMPONENT_TYPE_ENCODER) > 0)
	{
		menu_encode->AddEntry();
		menu_encode->AddEntry(i18n->TranslateString("Start encoding"), NIL, menu_encoders);
	}

	menu_encoder_options->AddEntry(i18n->TranslateString("Encode to single file"), NIL, NIL, (Bool *) &config->GetPersistentIntValue(Config::CategorySettingsID, Config::SettingsEncodeToSingleFileID, Config::SettingsEncodeToSingleFileDefault));

	menu_encoder_options->AddEntry();

	menu_encoder_options->AddEntry(i18n->TranslateString("Encode to input file folder if possible"), NIL, NIL, (Bool *) &config->GetPersistentIntValue(Config::CategorySettingsID, Config::SettingsWriteToInputDirectoryID, Config::SettingsWriteToInputDirectoryDefault))->onAction.Connect(&BonkEncGUI::ToggleUseInputDirectory, this);

	allowOverwriteMenuEntry = menu_encoder_options->AddEntry(i18n->TranslateString("Allow overwriting input file"), NIL, NIL, (Bool *) &config->GetPersistentIntValue(Config::CategorySettingsID, Config::SettingsAllowOverwriteSourceID, Config::SettingsAllowOverwriteSourceDefault));

	menu_encoder_options->AddEntry();

	menu_encoder_options->AddEntry(i18n->TranslateString("Delete original files after encoding"), NIL, NIL, &currentConfig->deleteAfterEncoding)->onAction.Connect(&BonkEncGUI::ConfirmDeleteAfterEncoding, this);

	menu_encoder_options->AddEntry();

	menu_encoder_options->AddEntry(i18n->TranslateString("Shutdown after encoding"), NIL, NIL, &currentConfig->shutdownAfterEncoding);

	menu_encode->AddEntry();

	menu_encode->AddEntry(i18n->TranslateString("Encoder options"), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:29")), menu_encoder_options);

	ToggleUseInputDirectory();

	i18n->SetContext("Menu::Help");

	entry = menu_help->AddEntry(i18n->TranslateString("Help topics..."), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:34")));
	entry->onAction.Connect(&BonkEncGUI::ShowHelp, this);
	entry->SetShortcut(0, Keyboard::KeyF1, mainWnd);

	menu_help->AddEntry();

	entry = menu_help->AddEntry(String(i18n->TranslateString("Show Tip of the Day")).Append("..."));
	entry->onAction.Connect(&BonkEncGUI::ShowTipOfTheDay, this);
	entry->SetShortcut(0, Keyboard::KeyF10, mainWnd);

	if (currentConfig->enable_eUpdate)
	{
		menu_help->AddEntry();
		menu_help->AddEntry(String(i18n->TranslateString("Check for updates now")).Append("..."))->onAction.Connect(&BonkEncGUI::CheckForUpdates, this);
		menu_help->AddEntry(i18n->TranslateString("Check for updates at startup"), NIL, NIL, (Bool *) &config->GetPersistentIntValue(Config::CategorySettingsID, Config::SettingsCheckForUpdatesID, Config::SettingsCheckForUpdatesDefault));
	}

	menu_help->AddEntry();
	menu_help->AddEntry(String(i18n->TranslateString("About %1")).Replace("%1", BonkEnc::appName).Append("..."), ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:35")))->onAction.Connect(&BonkEncGUI::About, this);

	i18n->SetContext("Menu");

	mainWnd_menubar->RemoveAllEntries();

	mainWnd_menubar->AddEntry(i18n->TranslateString("File"), NIL, menu_file);

	if (config->cdrip_numdrives >= 1) mainWnd_menubar->AddEntry(i18n->TranslateString("Database"), NIL, menu_database);

	mainWnd_menubar->AddEntry(i18n->TranslateString("Options"), NIL, menu_options);
	mainWnd_menubar->AddEntry(i18n->TranslateString("Encode"), NIL, menu_encode);

	mainWnd_menubar->AddEntry()->SetOrientation(OR_RIGHT);

	mainWnd_menubar->AddEntry(i18n->TranslateString("Help"), NIL, menu_help)->SetOrientation(OR_RIGHT);

	i18n->SetContext("Toolbar");

	mainWnd_iconbar->RemoveAllEntries();

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:1")), menu_files);
	entry->onAction.Connect(&JobList::AddTrackByDialog, joblist);
	entry->SetTooltipText(i18n->TranslateString("Add audio file(s) to the joblist"));

	if (config->cdrip_numdrives >= 1)
	{
		entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:2")), config->cdrip_numdrives > 1 ? menu_drives : NIL);
		entry->onAction.Connect(&BonkEncGUI::ReadCD, this);
		entry->SetTooltipText(i18n->TranslateString("Add audio CD contents to the joblist"));
	}

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:3")));
	entry->onAction.Connect(&JobList::RemoveSelectedTrack, joblist);
	entry->SetTooltipText(i18n->TranslateString("Remove the selected entry from the joblist"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:4")));
	entry->onAction.Connect(&JobList::StartJobRemoveAllTracks, joblist);
	entry->SetTooltipText(i18n->TranslateString("Clear the entire joblist"));

	if (config->cdrip_numdrives >= 1)
	{
		mainWnd_iconbar->AddEntry();

		entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:5")), menu_database_query);
		entry->onAction.Connect(&BonkEncGUI::QueryCDDB, this);
		entry->SetTooltipText(i18n->TranslateString("Query CDDB database"));

		entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:6")));
		entry->onAction.Connect(&BonkEncGUI::SubmitCDDBData, this);
		entry->SetTooltipText(i18n->TranslateString("Submit CDDB data..."));
	}

	mainWnd_iconbar->AddEntry();

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:7")), config->GetNOfConfigurations() > 1 ? menu_configurations : NIL);
	entry->onAction.Connect(&BonkEncGUI::ConfigureSettings, this);
	entry->SetTooltipText(i18n->TranslateString("Configure general settings"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:8")));
	entry->onAction.Connect(&BonkEncGUI::ConfigureEncoder, this);
	entry->SetTooltipText(i18n->TranslateString("Configure the selected audio encoder"));

	mainWnd_iconbar->AddEntry();

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:9")), boca.GetNumberOfComponentsOfType(COMPONENT_TYPE_ENCODER) > 0 ? menu_encoders : NIL);
	entry->onAction.Connect(&BonkEncGUI::Encode, this);
	entry->SetTooltipText(i18n->TranslateString("Start the encoding process"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:10")));
	entry->onAction.Connect(&BonkEncGUI::PauseResumeEncoding, this);
	entry->SetTooltipText(i18n->TranslateString("Pause/resume encoding"));

	entry = mainWnd_iconbar->AddEntry(NIL, ImageLoader::Load(String(bonkEncConfig->resourcesPath).Append("freac.pci:11")));
	entry->onAction.Connect(&BonkEncGUI::StopEncoding, this);
	entry->SetTooltipText(i18n->TranslateString("Stop encoding"));

	mainWnd_menubar->Show();
	mainWnd_iconbar->Show();
}

Void BonkEnc::BonkEncGUI::Encode()
{
	if (clicked_encoder >= 0)
	{
		BoCA::Config	*config = BoCA::Config::Get();
		Registry	&boca	= Registry::Get();

		config->SetStringValue(Config::CategorySettingsID, Config::SettingsEncoderID, boca.GetComponentID(clicked_encoder));

		tab_layer_joblist->UpdateEncoderText();

		clicked_encoder = -1;
	}

	encoder->Convert(joblist);
}

Void BonkEnc::BonkEncGUI::PauseResumeEncoding()
{
	if (!encoder->IsEncoding()) return;

	if (encoder->IsPaused()) encoder->Resume();
	else			 encoder->Pause();
}

Void BonkEnc::BonkEncGUI::StopEncoding()
{
	if (encoder == NIL) return;

	encoder->Stop();
}

Void BonkEnc::BonkEncGUI::AddFilesByPattern()
{
	AddPatternDialog	*dialog = new AddPatternDialog();

	if (dialog->ShowDialog() == Success()) joblist->AddTracksByPattern(dialog->GetDirectory(), dialog->GetPattern());

	DeleteObject(dialog);
}

Void BonkEnc::BonkEncGUI::AddFilesFromDirectory()
{
	AddDirectoryDialog	*dialog = new AddDirectoryDialog();

	if (dialog->ShowDialog() == Success())
	{
		Array<String>	 directories;

		directories.Add(dialog->GetDirectory());

		joblist->AddTracksByDragAndDrop(directories);
	}

	DeleteObject(dialog);
}

Void BonkEnc::BonkEncGUI::ToggleUseInputDirectory()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (config->GetIntValue(Config::CategorySettingsID, Config::SettingsWriteToInputDirectoryID, Config::SettingsWriteToInputDirectoryDefault)) allowOverwriteMenuEntry->Activate();
	else																	    allowOverwriteMenuEntry->Deactivate();
}

Void BonkEnc::BonkEncGUI::ToggleEncodeToSingleFile()
{
	BoCA::Config	*config = BoCA::Config::Get();

	if (config->GetIntValue(Config::CategorySettingsID, Config::SettingsEncodeToSingleFileID, Config::SettingsEncodeToSingleFileDefault)) config->SetIntValue(Config::CategorySettingsID, Config::SettingsEncodeOnTheFlyID, True);
}

Void BonkEnc::BonkEncGUI::ConfirmDeleteAfterEncoding()
{
	BoCA::I18n	*i18n = BoCA::I18n::Get();

	i18n->SetContext("Messages");

	if (currentConfig->deleteAfterEncoding)
	{
		if (Message::Button::No == QuickMessage(i18n->TranslateString("This option will remove the original files from your computer\nafter the encoding process!\n\nAre you sure you want to activate this function?"), i18n->TranslateString("Delete original files after encoding"), Message::Buttons::YesNo, Message::Icon::Question)) currentConfig->deleteAfterEncoding = False;
	}
}

Void BonkEnc::BonkEncGUI::ShowHelp()
{
	BoCA::I18n	*i18n = BoCA::I18n::Get();

	i18n->SetContext("Menu::Help");

	S::System::System::OpenURL(String("file://").Append(GUI::Application::GetApplicationDirectory()).Append(Config::Get()->documentationPath).Append("manual/").Append(i18n->TranslateString("index_en.html")));
}

Void BonkEnc::BonkEncGUI::ShowTipOfTheDay()
{
	BoCA::Config	*config = BoCA::Config::Get();
	BoCA::I18n	*i18n = BoCA::I18n::Get();

	i18n->SetContext("Tips");

	Bool		 showTips = config->GetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, Config::SettingsShowTipsDefault);
	TipOfTheDay	*dialog = new TipOfTheDay(&showTips);

	dialog->AddTip(String(i18n->TranslateString("%1 is available in %2 languages. If your language is\nnot available, you can easily translate %1 using the\n\'smooth Translator\' application.")).Replace("%1", BonkEnc::appName).Replace("%2", String::FromInt(Math::Max(36, i18n->GetNOfLanguages()))));
	dialog->AddTip(String(i18n->TranslateString("%1 comes with support for the LAME, Ogg Vorbis, FAAC,\nFLAC and Bonk encoders. An encoder for the VQF format is\navailable at the %1 website: %2")).Replace("%1", BonkEnc::appName).Replace("%2", BonkEnc::website));
	dialog->AddTip(String(i18n->TranslateString("%1 can use Winamp 2 input plug-ins to support more file\nformats. Copy the in_*.dll files to the %1/plugins directory to\nenable %1 to read these formats.")).Replace("%1", BonkEnc::appName));
	dialog->AddTip(String(i18n->TranslateString("With %1 you can submit freedb CD database entries\ncontaining Unicode characters. So if you have any CDs with\nnon-Latin artist or title names, you can submit the correct\nfreedb entries with %1.")).Replace("%1", BonkEnc::appName));
	dialog->AddTip(String(i18n->TranslateString("To correct reading errors while ripping you can enable\nJitter correction in the CDRip tab of %1's configuration\ndialog. If that does not help, try using one of the Paranoia modes.")).Replace("%1", BonkEnc::appName));
	dialog->AddTip(String(i18n->TranslateString("Do you have any suggestions on how to improve %1?\nYou can submit any ideas through the Tracker on the %1\nSourceForge project page - %2\nor send an eMail to %3.")).Replace("%1", BonkEnc::appName).Replace("%2", "http://sf.net/projects/bonkenc").Replace("%3", "suggestions@freac.org"));
	dialog->AddTip(String(i18n->TranslateString("Do you like %1? %1 is available for free, but you can\nhelp fund the development by donating to the %1 project.\nYou can send money to %2 through PayPal.\nSee %3 for more details.")).Replace("%1", BonkEnc::appName).Replace("%2", "donate@freac.org").Replace("%3", String(BonkEnc::website).Append("donating.php")));

	dialog->SetMode(TIP_ORDERED, config->GetIntValue(Config::CategorySettingsID, Config::SettingsNextTipID, Config::SettingsNextTipDefault), config->GetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, Config::SettingsShowTipsDefault));

	dialog->ShowDialog();

	config->SetIntValue(Config::CategorySettingsID, Config::SettingsShowTipsID, showTips);
	config->SetIntValue(Config::CategorySettingsID, Config::SettingsNextTipID, dialog->GetOffset());

	DeleteObject(dialog);
}

Void BonkEnc::BonkEncGUI::CheckForUpdates()
{
	(new JobCheckForUpdates(False))->Schedule();
}