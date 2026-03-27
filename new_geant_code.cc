#include <G4Box.hh>
#include <G4Tubs.hh>
#include <G4NistManager.hh>
#include <G4ParticleGun.hh>
#include <G4ParticleTable.hh>
#include <G4ParticleDefinition.hh>
#include <G4PVPlacement.hh>
#include <G4RunManager.hh>
#include <G4Event.hh>
#include <G4SDManager.hh>
#include <G4VisExecutive.hh>
#include <G4VisManager.hh>
#include <G4VSensitiveDetector.hh>
#include <G4VUserDetectorConstruction.hh>
#include <G4VUserPrimaryGeneratorAction.hh>
#include <G4UIExecutive.hh>
#include <G4UImanager.hh>
#include <G4UIsession.hh>
#include <G4UserEventAction.hh>
#include <FTFP_BERT.hh>
#include <G4RandomDirection.hh>
#include <TFile.h>
#include <TNtuple.h>
#include <TCanvas.h>
#include <TProfile.h>
#include <TF1.h>
#include <TStyle.h>
#include <TLegend.h>
#include "Randomize.hh"
using namespace std;
using CLHEP::cm;
using CLHEP::degree;
using CLHEP::MeV;
using CLHEP::mm;

class sensitive_detector : public G4VSensitiveDetector
{
private:
  double edep, length;
  TNtuple *ntuple;

public:
  sensitive_detector(G4String name) : G4VSensitiveDetector(name)
  {
    ntuple = new TNtuple("hits", "detector hits", "energy:edep:length:dedx");
  }

  virtual ~sensitive_detector()
  {
    if (ntuple)
    {
      ntuple->Write();
      delete ntuple;
    }
  }

  virtual void Initialize(G4HCofThisEvent *)
  {
    edep = 0.0;
    length = 0.0;
  }

  G4bool ProcessHits(G4Step *step, G4TouchableHistory *)
  {
    G4Track *track = step->GetTrack();
    G4ParticleDefinition *particle = track->GetDefinition();
    if (particle == G4ParticleTable::GetParticleTable()->FindParticle("mu-"))
    {
      edep += step->GetTotalEnergyDeposit();
      length += step->GetStepLength();
    }
    return true;
  }

  virtual void EndOfEvent(G4HCofThisEvent *)
  {
    int e = G4RunManager::GetRunManager()->GetCurrentEvent()->GetEventID();
    const G4Event *ev = G4RunManager::GetRunManager()->GetCurrentEvent();
    G4PrimaryVertex *pv = ev->GetPrimaryVertex();
    G4PrimaryParticle *pp = pv->GetPrimary();
    G4double ekin = pp->GetKineticEnergy();
    if (edep > 0 && length > 0)
    {
      double dedx = edep / length; // МэВ/мм
      G4cout << ">>> event " << e
             << " energy=" << ekin / MeV << " MeV"
             << " edep=" << edep / MeV << " MeV"
             << " length=" << length / cm << " cm"
             << " dE/dx=" << dedx / (MeV / cm) << " MeV/cm"
             << G4endl;
      ntuple->Fill(ekin / MeV, edep / MeV, length / cm, dedx / (MeV / cm));
    }
  }
};

struct det_constr : G4VUserDetectorConstruction
{
  G4VPhysicalVolume *Construct()
  {
    G4Box *world_box = new G4Box("world_box", 500 * mm, 500 * mm, 500 * mm);
    G4Material *water = G4NistManager::Instance()->FindOrBuildMaterial("G4_WATER");
    G4LogicalVolume *world_logical_volume = new G4LogicalVolume(world_box, water, "world_logical_vol");
    G4VPhysicalVolume *world_physical_volume = new G4PVPlacement(0, G4ThreeVector(), world_logical_volume, "world_physical_vol", 0, false, 0);

    sensitive_detector *detector = new sensitive_detector("detector");
    G4SDManager::GetSDMpointer()->AddNewDetector(detector);
    world_logical_volume->SetSensitiveDetector(detector);

    return world_physical_volume;
  }
};

struct pga : G4VUserPrimaryGeneratorAction
{
  G4ParticleGun *particle_gun;
  pga()
  {
    this->particle_gun = new G4ParticleGun(1);
  }

  ~pga()
  {
    delete this->particle_gun;
  }

  void GeneratePrimaries(G4Event *event)
  {
    G4double energy = G4UniformRand() * (100 * MeV - 1 * MeV) + 1 * MeV;
    this->particle_gun->SetParticleEnergy(energy);
    this->particle_gun->GeneratePrimaryVertex(event);
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

  delete rm;
  // file->Close();
  delete file;

  TFile *f = TFile::Open("filename.root");
  if (f && !f->IsZombie())
  {
    TNtuple *nt = (TNtuple *)f->Get("hits");
    if (nt)
    {
      TProfile *prof = new TProfile("prof", "dE/dx for muons in water;Energy (MeV);dE/dx (MeV/cm)",
                                    50, 1, 100);
      nt->Project("prof", "dedx:energy");
      prof->SetErrorOption("");
      prof->SetMarkerStyle(20);

      TF1 *bethe = new TF1("bethe", [](double *x, double *)
                           {
                double E = x[0];
                double mmu = 105.7; 
                double me = 0.511;      
                double I = 13.5e-6;         
                double C = 0.170;
                double gamma = 1 + E / mmu;
                double beta2 = 1 - 1/(gamma*gamma);
                double term = log(2*me*beta2*gamma*gamma / I) - beta2;
                return C / beta2 * term; }, 1, 100, 0);
      bethe->SetLineColor(kRed);
      bethe->SetLineWidth(2);

      TCanvas *c = new TCanvas("c", "dE/dx fit", 800, 600);
      prof->Draw("P");
      bethe->Draw("same");
      TLegend *leg = new TLegend(0.7, 0.8, 0.9, 0.9);
      leg->AddEntry(prof, "Geant4 data", "p");
      leg->AddEntry(bethe, "Bethe-Bloch theory", "l");
      leg->Draw();
      c->SaveAs("dedx_theory.png");
      delete c;
      delete leg;
    }
    delete f;
  }

  return 0;
}
