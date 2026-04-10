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
    double e_entry;
    double e_exit;
    double path;
    bool inside;
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

        if (!inside && pre->GetPhysicalVolume()->GetLogicalVolume()->GetSensitiveDetector() == this)
        {
            inside = true;
            e_entry = pre->GetKineticEnergy();
        }
        if (inside)
            path += step->GetStepLength();

        if (inside && post->GetPhysicalVolume()->GetLogicalVolume()->GetSensitiveDetector() != this)
        {
            e_exit = post->GetKineticEnergy();
            double dEdx = (e_entry - e_exit) / path;
            ntuple->Fill(e_entry / MeV, e_exit / MeV, dEdx / (MeV / cm), path / cm);
            inside = false;
        }
        return true;
    }
};

struct det_constr : G4VUserDetectorConstruction
{
    G4VPhysicalVolume *Construct() override
    {

        G4Box *world_box = new G4Box("world", 100 * cm, 100 * cm, 100 * cm);
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

struct pga : G4VUserPrimaryGeneratorAction
{
    G4ParticleGun *gun;
    pga()
    {
        gun = new G4ParticleGun(1);
        G4ParticleDefinition *electron = G4ParticleTable::GetParticleTable()->FindParticle("e-");
        gun->SetParticleDefinition(electron);
        gun->SetParticleEnergy(250 * MeV);
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

    TFile *f = TFile::Open("filename.root");
    if (f && !f->IsZombie())
    {
        TNtuple *nt = (TNtuple *)f->Get("hits");
        if (nt)
        {

            TH1F *h_dEdx = new TH1F("h_dEdx", "dE/dx for 250 MeV e- in Ar;dE/dx (MeV/cm);Events",
                                    100, 0, 0.014);
            nt->Draw("dEdx>>h_dEdx");

            double mean = h_dEdx->GetMean();
            cout << "Mean energy loss = " << mean * 1000 << " keV" << endl;
            // TF1 *new_graph = new TF1("new_graph", "new_graph", 0, 30);
            // new_graph->SetParameters(h_dEdx->Integral(), mean, 1.0);

            TCanvas *c = new TCanvas("c", "dE/dx distribution", 800, 600);
            h_dEdx->Draw();
            c->SaveAs("dEdx_hist.png");

            double E = 250.0;
            double m_e = 0.511;
            double gamma = 1 + E / m_e;
            double beta2 = 1 - 1 / (gamma * gamma);
            // double beta = sqrt(beta2);
            double I = 188.0e-6;
            double K = 0.307;
            double Z = 18.0, A = 39.95;
            double C = K * Z / A;
            double rho = 1.782e-3;

            double dedx_theory = C / beta2 * (0.5 * log(2 * m_e * beta2 * gamma * gamma / I) - beta2);
            dedx_theory *= rho;

            cout << "Theoretical dE/dx (Bethe-Bloch) = " << dedx_theory << " MeV/cm" << endl;

            delete c;
            delete h_dEdx;
        }
        delete f;
    }

    return 0;
}

// h_dEdx->Fit("new_graph", "R");
// double mpv = new_graph->GetParameter(1);
// cout << "Most Probable energy loss = " << mpv << endl;