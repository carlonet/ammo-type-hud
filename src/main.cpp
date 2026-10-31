// Munizioni nell'HUD: scrive il tipo di munizioni dell'arma impugnata accanto al contatore dei colpi
// (in basso a destra), nel colore e nel carattere dell'HUD del gioco; e il nome della granata o mina
// equipaggiata accanto al contatore delle granate.
//
// Il campo di testo viene aggiunto dentro AmmoCount_mc di HUDMenu.swf, così sparisce insieme al
// contatore (armi da mischia, menu, Pip-Boy). Ogni 250 ms un compito sul thread dell'interfaccia
// controlla le munizioni attuali e aggiorna il testo solo se sono cambiate.
// Posizione, dimensione e parole da togliere dal nome si regolano in Data\F4SE\Plugins\AmmoTypeHUD.ini,
// anche a gioco aperto.

namespace
{
	namespace GFx = Scaleform::GFx;


	struct Impostazioni
	{
		double dimensione{ 20.0 };  // dimensione del carattere (i numeri del contatore sono 32)
		double destra{ -56.0 };     // bordo destro del testo, rispetto al contatore
		double alto{ 4.0 };         // bordo superiore del testo, rispetto al contatore (riga della riserva)
		double larghezza{ 320.0 };
		// parole tolte all'inizio o alla fine del nome: "Munizioni 10mm" -> "10mm", ".50 Caliber" -> ".50"
		std::vector<std::string> togli{ "Munizioni ", "Calibro ", " Round", " Caliber" };
		// nome e numero della granata o mina equipaggiata, sulla riga sopra le munizioni
		bool   granata{ true };
		double granataDestra{ -56.0 };
		double granataAlto{ -33.0 };  // riga del caricatore
	};

	Impostazioni           impostazioni;
	std::filesystem::path  fileIni;
	std::filesystem::file_time_type dataIni{};
	std::atomic_bool       partitaCaricata{ false };
	bool                   ricreaCampo{ false };

	double LeggiNumero(const wchar_t* a_chiave, double a_predefinito)
	{
		wchar_t buf[64]{};
		GetPrivateProfileStringW(L"HUD", a_chiave, L"", buf, 64, fileIni.c_str());
		try {
			return buf[0] ? std::stod(buf) : a_predefinito;
		} catch (...) {
			return a_predefinito;
		}
	}

	std::string InUtf8(const wchar_t* a_testo)
	{
		std::string utf8(static_cast<std::size_t>(WideCharToMultiByte(CP_UTF8, 0, a_testo, -1, nullptr, 0, nullptr, nullptr)), '\0');
		WideCharToMultiByte(CP_UTF8, 0, a_testo, -1, utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
		utf8.resize(std::strlen(utf8.c_str()));
		return utf8;
	}

	// Elenco separato da "|"; gli spazi contano ("Munizioni " toglie anche lo spazio dopo la parola)
	std::vector<std::string> LeggiElenco(const wchar_t* a_chiave, const std::vector<std::string>& a_predefinito)
	{
		wchar_t buf[1024]{};
		if (GetPrivateProfileStringW(L"HUD", a_chiave, L"\x1", buf, 1024, fileIni.c_str()) && buf[0] == L'\x1') {
			return a_predefinito;  // chiave assente
		}
		std::vector<std::string> elenco;
		std::string tutto = InUtf8(buf);
		std::size_t inizio = 0;
		while (inizio <= tutto.size()) {
			const auto fine = std::min(tutto.find('|', inizio), tutto.size());
			if (fine > inizio) {
				elenco.push_back(tutto.substr(inizio, fine - inizio));
			}
			inizio = fine + 1;
		}
		return elenco;
	}

	// Rilegge il file .ini quando cambia: si può regolare la posizione senza riavviare il gioco.
	void AggiornaImpostazioni()
	{
		std::error_code err;
		const auto data = std::filesystem::last_write_time(fileIni, err);
		if (err || data == dataIni) {
			return;
		}
		dataIni = data;
		const Impostazioni base;
		impostazioni.dimensione = LeggiNumero(L"Size", base.dimensione);
		impostazioni.destra = LeggiNumero(L"Right", base.destra);
		impostazioni.alto = LeggiNumero(L"Top", base.alto);
		impostazioni.larghezza = LeggiNumero(L"Width", base.larghezza);
		impostazioni.togli = LeggiElenco(L"Strip", base.togli);
		impostazioni.granata = LeggiNumero(L"Grenade", 1.0) != 0.0;
		impostazioni.granataDestra = LeggiNumero(L"GrenadeRight", base.granataDestra);
		impostazioni.granataAlto = LeggiNumero(L"GrenadeTop", base.granataAlto);
		ricreaCampo = true;
		REX::INFO("Settings: size {}, right {}, top {}", impostazioni.dimensione, impostazioni.destra, impostazioni.alto);
	}

	// I nomi del gioco dovrebbero essere già in UTF-8; se non lo sono (Windows-1252), si convertono.
	std::string InUtf8(const char* a_testo)
	{
		const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a_testo, -1, nullptr, 0);
		if (n > 0) {
			return a_testo;
		}
		std::wstring largo(static_cast<std::size_t>(MultiByteToWideChar(1252, 0, a_testo, -1, nullptr, 0)), L'\0');
		MultiByteToWideChar(1252, 0, a_testo, -1, largo.data(), static_cast<int>(largo.size()));
		std::string utf8(static_cast<std::size_t>(WideCharToMultiByte(CP_UTF8, 0, largo.c_str(), -1, nullptr, 0, nullptr, nullptr)), '\0');
		WideCharToMultiByte(CP_UTF8, 0, largo.c_str(), -1, utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
		utf8.resize(std::strlen(utf8.c_str()));
		return utf8;
	}

	// Nome da mostrare: senza etichette dei riordinatori e senza le parole dell'elenco Strip
	std::string NomeBreve(const RE::TESForm& a_oggetto)
	{
		const auto nome = RE::TESFullName::GetFullName(a_oggetto);
		if (nome.empty()) {
			return {};
		}
		std::string testo = InUtf8(std::string(nome).c_str());
		// etichette dei riordinatori di oggetti, per esempio "[Ammo] "
		if (testo.starts_with('[')) {
			if (const auto fine = testo.find(']'); fine != std::string::npos) {
				testo.erase(0, testo.find_first_not_of(' ', fine + 1));
			}
		}
		for (const auto& parola : impostazioni.togli) {
			if (testo.size() <= parola.size()) {
				continue;
			}
			if (testo.starts_with(parola)) {
				testo.erase(0, parola.size());
			} else if (testo.ends_with(parola)) {
				testo.erase(testo.size() - parola.size());
			}
		}
		return testo;
	}

	std::string NomeMunizioni()
	{
		const auto giocatore = RE::PlayerCharacter::GetSingleton();
		if (!giocatore) {
			return {};
		}
		RE::BGSEquipIndex indice;
		indice.index = 0;
		const auto munizioni = giocatore->GetCurrentAmmo(indice);
		return munizioni ? NomeBreve(*munizioni) : std::string{};
	}

	// Granata o mina equipaggiata e quante se ne hanno, per esempio "Molotov 2": nell'inventario,
	// l'arma da lancio con una pila equipaggiata (il gioco non la tiene tra le armi in mano)
	std::string NomeGranata()
	{
		const auto giocatore = RE::PlayerCharacter::GetSingleton();
		if (!giocatore || !giocatore->inventoryList) {
			return {};
		}
		auto& inventario = *giocatore->inventoryList;
		RE::BSAutoReadLock blocco{ inventario.rwLock };
		for (const auto& voce : inventario.data) {
			const auto arma = voce.object ? voce.object->As<RE::TESObjectWEAP>() : nullptr;
			// Il tipo è un valore, non un insieme di bit: IsThrownWeapon() di CommonLibF4 lo confronta
			// come maschera e scambia anche le pistole (9) per granate (10) e mine (11).
			const auto tipo = arma ? arma->weaponData.type.get() : RE::WEAPON_TYPE::kHandToHand;
			if (tipo != RE::WEAPON_TYPE::kGrenade && tipo != RE::WEAPON_TYPE::kMine) {
				continue;
			}
			bool equipaggiata = false;
			std::uint32_t quante = 0;
			for (auto pila = voce.stackData.get(); pila; pila = pila->nextStack.get()) {
				equipaggiata = equipaggiata || pila->IsEquipped();
				quante += pila->GetCount();
			}
			if (equipaggiata) {
				const auto nome = NomeBreve(*arma);
				return nome.empty() ? nome : std::format("{} {}", nome, quante);
			}
		}
		return {};
	}

	// Un campo di testo aggiunto a un contatore dell'HUD, accanto ai suoi numeri
	struct Campo
	{
		const char* nome;          // nome della proprietà nel contatore
		const char* contatore;     // contatore dentro RightMeters_mc
		const char* modello;       // campo di testo del contatore di cui copiare il carattere
		std::string ultimoTesto;
	};

	Campo campoMunizioni{ "AmmoTypeHUD_tf", "AmmoCount_mc", "ReserveCount_tf", {} };
	// la granata sta nel contatore dei colpi, sulla riga del caricatore: il contatore delle granate del
	// gioco compare solo per pochi secondi quando si cambia arma
	Campo campoGranata{ "AmmoTypeHUD_grenade_tf", "AmmoCount_mc", "ClipCount_tf", {} };

	// Crea il campo se manca (HUD appena caricato) o se sono cambiate le impostazioni, poi aggiorna il testo
	void AggiornaCampo(GFx::Movie& a_film, GFx::Value& a_destro, Campo& a_campo, bool a_ricrea, bool a_attivo,
		double a_destra, double a_alto, const std::string& a_testo)
	{
		GFx::Value contatore, modello, campo;
		if (!a_destro.GetMember(a_campo.contatore, &contatore) || !contatore.IsDisplayObject()) {
			return;
		}
		const bool esiste = contatore.GetMember(a_campo.nome, &campo) && campo.IsDisplayObject();
		if (esiste && (a_ricrea || !a_attivo)) {
			contatore.Invoke("removeChild", nullptr, &campo, 1);
			contatore.SetMember(a_campo.nome, GFx::Value());
		}
		if (!a_attivo) {
			return;
		}
		if (!esiste || a_ricrea) {
			if (!contatore.GetMember(a_campo.modello, &modello) || !modello.IsDisplayObject()) {
				return;
			}
			a_film.CreateObject(&campo, "flash.text.TextField");
			if (!campo.IsDisplayObject()) {
				REX::WARN("Could not create the text field {}", a_campo.nome);
				return;
			}
			// Stesso carattere del contatore (incorporato nel file dell'HUD), più piccolo e allineato a destra
			GFx::Value formato, incorporato;
			modello.Invoke("getTextFormat", &formato);
			if (formato.IsObject()) {
				formato.SetMember("size", GFx::Value(impostazioni.dimensione));
				formato.SetMember("align", GFx::Value("right"));
				campo.SetMember("defaultTextFormat", formato);
			}
			modello.GetMember("embedFonts", &incorporato);
			campo.SetMember("embedFonts", incorporato);
			campo.SetMember("selectable", GFx::Value(false));
			campo.SetMember("mouseEnabled", GFx::Value(false));
			campo.SetMember("width", GFx::Value(impostazioni.larghezza));
			campo.SetMember("height", GFx::Value(impostazioni.dimensione * 1.6));
			campo.SetMember("x", GFx::Value(a_destra - impostazioni.larghezza));
			campo.SetMember("y", GFx::Value(a_alto));
			contatore.Invoke("addChild", nullptr, &campo, 1);
			contatore.SetMember(a_campo.nome, campo);
			a_campo.ultimoTesto.clear();
			campo.SetMember("text", GFx::Value(""));
			REX::INFO("Field {} added to the HUD", a_campo.nome);
		}
		if (a_testo != a_campo.ultimoTesto) {
			a_campo.ultimoTesto = a_testo;
			campo.SetMember("text", GFx::Value(a_campo.ultimoTesto.c_str()));
		}
	}

	// Sul thread dell'interfaccia
	void AggiornaHUD()
	{
		const auto ui = RE::UI::GetSingleton();
		if (!ui) {
			return;
		}
		const auto menu = ui->GetMenu("HUDMenu"sv);
		if (!menu || !menu->uiMovie || !menu->menuObj.IsObject()) {
			return;
		}
		GFx::Value destro;
		if (!menu->menuObj.GetMember("RightMeters_mc", &destro) || !destro.IsDisplayObject()) {
			return;
		}
		const bool ricrea = ricreaCampo;
		ricreaCampo = false;
		AggiornaCampo(*menu->uiMovie, destro, campoMunizioni, ricrea, true,
			impostazioni.destra, impostazioni.alto, NomeMunizioni());
		AggiornaCampo(*menu->uiMovie, destro, campoGranata, ricrea, impostazioni.granata,
			impostazioni.granataDestra, impostazioni.granataAlto, impostazioni.granata ? NomeGranata() : std::string{});
	}

	void Controllo(std::stop_token a_stop)
	{
		while (!a_stop.stop_requested()) {
			std::this_thread::sleep_for(250ms);
			if (!partitaCaricata) {
				continue;
			}
			AggiornaImpostazioni();
			if (const auto compiti = F4SE::GetTaskInterface()) {
				compiti->AddUITask([]() { AggiornaHUD(); });
			}
		}
	}

	std::jthread controllo;

	void Messaggio(F4SE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case F4SE::MessagingInterface::kGameDataReady:
			if (!controllo.joinable()) {
				controllo = std::jthread(Controllo);
			}
			break;
		case F4SE::MessagingInterface::kPostLoadGame:
		case F4SE::MessagingInterface::kNewGame:
			partitaCaricata = true;
			break;
		default:
			break;
		}
	}
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	wchar_t percorso[MAX_PATH]{};
	GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), percorso, MAX_PATH);
	fileIni = std::filesystem::path(percorso).replace_extension(L".ini");
	AggiornaImpostazioni();

	if (const auto messaggi = F4SE::GetMessagingInterface()) {
		messaggi->RegisterListener(Messaggio);
	}
	REX::INFO("Ammo Type on HUD loaded");
	return true;
}
