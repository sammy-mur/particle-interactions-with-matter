#include <G4Box.hh>
#include <G4NistManager.hh>
#include <G4ParticleGun.hh>
#include <G4ParticleTable.hh>
#include <G4PVPlacement.hh>
#include <G4RunManager.hh>
#include <G4Event.hh>
#include <G4SDManager.hh>
#include <G4VisExecutive.hh>
#include <G4UIExecutive.hh>
#include <G4UImanager.hh>
#include <FTFP_BERT.hh>
#include <G4VUserDetectorConstruction.hh>
#include <G4VUserPrimaryGeneratorAction.hh>
#include <TFile.h>
#include <TNtuple.h>
#include <TCanvas.h>
#include <TH1F.h>
#include <TF1.h>

using namespace std;
using CLHEP::cm, CLHEP::MeV, CLHEP::mm;

class sensitive_detector : public G4VSensitiveDetector
{
private:
    double e_entry; // энергия при входе (МэВ)
    double e_exit;  // энергия при выходе (МэВ)
    double path;    // путь внутри детектора (мм)
    bool inside;    // флаг нахождения внутри
    TNtuple *ntuple;

public:
    sensitive_detector(G4String name) : G4VSensitiveDetector(name)
    {
        ntuple = new TNtuple("hits", "detector hits", "E_entry:E_exit:dEdx:path");
    }

    virtual ~sensitive_detector()
    {
        if (ntuple)
        {
            ntuple->Write();
            delete ntuple;
        }
    }

    virtual void Initialize(G4HCofThisEvent *) override
    {
        inside = false;
        e_entry = 0.0;
        e_exit = 0.0;
        path = 0.0;
    }

    G4bool ProcessHits(G4Step *step, G4TouchableHistory *) override
    {
        G4Track *track = step->GetTrack();
        G4ParticleDefinition *particle = track->GetDefinition();
        if (particle != G4ParticleTable::GetParticleTable()->FindParticle("e-"))
            return true;

        G4StepPoint *pre = step->GetPreStepPoint();
        G4StepPoint *post = step->GetPostStepPoint();

        // Вход в детектор (ещё не внутри, и текущая точка – внутри чувствительного объёма)
        if (!inside && pre->GetPhysicalVolume()->GetLogicalVolume()->GetSensitiveDetector() == this)
        {
            inside = true;
            e_entry = pre->GetKineticEnergy();
        }

        // Если внутри, накапливаем длину шага
        if (inside)
            path += step->GetStepLength();

        // Выход из детектора (были внутри и следующий шаг вне чувствительного объёма)
        if (inside && post->GetPhysicalVolume()->GetLogicalVolume()->GetSensitiveDetector() != this)
        {
            e_exit = post->GetKineticEnergy();
            double dEdx = (e_entry - e_exit) / path; // МэВ/мм
            ntuple->Fill(e_entry / MeV, e_exit / MeV, dEdx / (MeV / cm), path / cm);
            inside = false;
        }
        return true;
    }
};

// ----------------- Геометрия с вашими размерами -----------------
struct det_constr : G4VUserDetectorConstruction
{
    G4VPhysicalVolume *Construct() override
    {
        // Мир – вакуум (Galactic), размером чуть больше слоя
        G4Box *world_box = new G4Box("world", 200 * cm, 200 * cm, 200 * cm);
        G4Material *vacuum = G4NistManager::Instance()->FindOrBuildMaterial("G4_Galactic");
        G4LogicalVolume *world_log = new G4LogicalVolume(world_box, vacuum, "world_log");
        G4VPhysicalVolume *world_phys = new G4PVPlacement(0, G4ThreeVector(), world_log, "world", 0, false, 0);

        G4Material *argon = G4NistManager::Instance()->FindOrBuildMaterial("G4_Ar");
        G4Box *ar_layer = new G4Box("Ar_layer", 0.5 * cm, 100 * cm, 100 * cm);
        G4LogicalVolume *layer_log = new G4LogicalVolume(ar_layer, argon, "layer_log");
        new G4PVPlacement(0, G4ThreeVector(0, 0, 0), layer_log, "layer_phys", world_log, false, 0);

        // Чувствительный детектор
        sensitive_detector *det = new sensitive_detector("ArDetector");
        G4SDManager::GetSDMpointer()->AddNewDetector(det);
        layer_log->SetSensitiveDetector(det);

        return world_phys;
    }
};

// ----------------- Генератор частиц (электроны 250 МэВ, летят вдоль Z) -----------------
struct pga : G4VUserPrimaryGeneratorAction
{
    G4ParticleGun *gun;
    pga()
    {
        gun = new G4ParticleGun(1);
        G4ParticleDefinition *electron = G4ParticleTable::GetParticleTable()->FindParticle("e-");
        gun->SetParticleDefinition(electron);
        gun->SetParticleEnergy(250 * MeV);
        // Стартуем перед слоем: Z = -150 см (слой занимает Z от -100 до +100 см)
        gun->SetParticlePosition(G4ThreeVector(0, 0, -150 * cm));
        gun->SetParticleMomentumDirection(G4ThreeVector(0, 0, 1)); // вдоль +Z
    }
    ~pga() { delete gun; }
    void GeneratePrimaries(G4Event *event) override
    {
        gun->GeneratePrimaryVertex(event);
    }
};

int main(int argc, char *argv[])
{
    TFile *file = new TFile("filename.root", "RECREATE");

    G4RunManager *rm = new G4RunManager;
    rm->SetUserInitialization(new det_constr);
    rm->SetUserInitialization(new FTFP_BERT);
    rm->SetUserAction(new pga);
    rm->Initialize();

    G4UImanager *ui = G4UImanager::GetUIpointer();
    G4VisManager *vm = new G4VisExecutive;
    vm->Initialize();
    G4UIExecutive *uie = new G4UIExecutive(argc, argv);
    ui->ApplyCommand("/control/execute vis.mac");
    uie->SessionStart();

    delete uie;
    delete vm;
    delete rm;
    file->Close();
    delete file;

    // Анализ: гистограмма dE/dx
    TFile *f = TFile::Open("filename.root");
    if (f && !f->IsZombie())
    {
        TNtuple *nt = (TNtuple *)f->Get("hits");
        if (nt)
        {
            // Гистограмма dE/dx (в МэВ/мм)
            TH1F *h_dEdx = new TH1F("h_dEdx", "dE/dx for 250 MeV e- in Ar;dE/dx (MeV/cm);Events",
                                    100, 0, 0.5); // диапазон подберите по данным
            nt->Draw("dEdx>>h_dEdx");

            // Рисуем и сохраняем
            TCanvas *c = new TCanvas("c", "dE/dx distribution", 800, 600);
            h_dEdx->Draw();
            c->SaveAs("dEdx_hist.png");

            delete c;
            delete h_dEdx;
        }
        delete f;
    }

    return 0;
}