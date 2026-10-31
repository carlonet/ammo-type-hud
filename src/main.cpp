// Munizioni nell'HUD: scrive il tipo di munizioni dell'arma impugnata accanto al contatore dei colpi
// (in basso a destra), nel colore e nel carattere dell'HUD del gioco.
//
// Il campo di testo viene aggiunto dentro AmmoCount_mc di HUDMenu.swf, così sparisce insieme al
// contatore (armi da mischia, menu, Pip-Boy). Ogni 250 ms un compito sul thread dell'interfaccia
// controlla le munizioni attuali e aggiorna il testo solo se sono cambiate.
// Posizione, dimensione e parole da togliere dal nome si regolano in Data\F4SE\Plugins\AmmoTypeHUD.ini,
// anche a gioco aperto.

namespace
{
	namespace GFx = Scaleform::GFx;

	constexpr auto NOME_CAMPO = "AmmoTypeHUD_tf";

	struct Impostazioni
	{
		double dimensione{ 20.0 };  // dimensione del carattere (i numeri del contatore sono 32)
		double destra{ -56.0 };     // bordo destro del testo, rispetto al contatore
		double alto{ -11.0 };       // bordo superiore del testo, rispetto al contatore
		double larghezza{ 320.0 };
		// parole tolte all'inizio o alla fine del nome: "Munizioni 10mm" -> "10mm", ".50 Caliber" -> ".50"
		std::vector<std::string> togli{ "Munizioni ", "Calibro ", " Round", " Caliber" };
	};

	Impostazioni           impostazioni;
	std::filesystem::path  fileIni;
	std::filesystem::file_time_type dataIni{};
	std::atomic_bool       partitaCaricata{ false };
	std::string            ultimoTesto;
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

	std::string NomeMunizioni()
	{
		const auto giocatore = RE::PlayerCharacter::GetSingleton();
		if (!giocatore) {
			return {};
		}
		RE::BGSEquipIndex indice;
		indice.index = 0;
		const auto munizioni = giocatore->GetCurrentAmmo(indice);
		if (!munizioni) {
			return {};
		}
		const auto nome = RE::TESFullName::GetFullName(*munizioni);
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

	// Sul thread dell'interfaccia: crea il campo se manca (HUD appena caricato) e aggiorna il testo.
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

		GFx::Value destro, contatore, riserva, campo;
		if (!menu->menuObj.GetMember("RightMeters_mc", &destro) || !destro.IsDisplayObject() ||
			!destro.GetMember("AmmoCount_mc", &contatore) || !contatore.IsDisplayObject()) {
			return;
		}

		const bool esiste = contatore.GetMember(NOME_CAMPO, &campo) && campo.IsDisplayObject();
		if (esiste && ricreaCampo) {
			contatore.Invoke("removeChild", nullptr, &campo, 1);
		}
		if (!esiste || ricreaCampo) {
			ricreaCampo = false;
			if (!contatore.GetMember("ReserveCount_tf", &riserva) || !riserva.IsDisplayObject()) {
				return;
			}
			menu->uiMovie->CreateObject(&campo, "flash.text.TextField");
			if (!campo.IsDisplayObject()) {
				REX::WARN("Could not create the text field");
				return;
			}
			// Stesso carattere del contatore (incorporato nel file dell'HUD), più piccolo e allineato a destra
			GFx::Value formato, incorporato;
			riserva.Invoke("getTextFormat", &formato);
			if (formato.IsObject()) {
				formato.SetMember("size", GFx::Value(impostazioni.dimensione));
				formato.SetMember("align", GFx::Value("right"));
				campo.SetMember("defaultTextFormat", formato);
			}
			riserva.GetMember("embedFonts", &incorporato);
			campo.SetMember("embedFonts", incorporato);
			campo.SetMember("selectable", GFx::Value(false));
			campo.SetMember("mouseEnabled", GFx::Value(false));
			campo.SetMember("width", GFx::Value(impostazioni.larghezza));
			campo.SetMember("height", GFx::Value(impostazioni.dimensione * 1.6));
			campo.SetMember("x", GFx::Value(impostazioni.destra - impostazioni.larghezza));
			campo.SetMember("y", GFx::Value(impostazioni.alto));
			contatore.Invoke("addChild", nullptr, &campo, 1);
			contatore.SetMember(NOME_CAMPO, campo);
			ultimoTesto.clear();
			campo.SetMember("text", GFx::Value(""));
			REX::INFO("Ammo type field added to the HUD");
		}

		const auto testo = NomeMunizioni();
		if (testo != ultimoTesto) {
			ultimoTesto = testo;
			campo.SetMember("text", GFx::Value(ultimoTesto.c_str()));
		}
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
