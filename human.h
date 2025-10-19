#pragma once
#include "Delegates.h"  // ���������
#include "Car.h"        // ��������� ��� List<Car^>

using namespace System;
using namespace System::Drawing;
using namespace System::Windows::Forms;
using namespace System::IO; // ��� Path::Combine ��� �������������
using namespace System::Collections::Generic; // ���������

// ��������� forward declaration ����� �������� ����������� ������������
ref class Car;  // ��������� ��� ������

public ref class Human
{
protected:
    PictureBox^ sprite;
    String^ name;
    String^ role;

public:
    Human(String^ name, String^ role, String^ imagePath) : name(name), role(role)
    {
        sprite = gcnew PictureBox();
        sprite->SizeMode = PictureBoxSizeMode::AutoSize;
        if (!String::IsNullOrEmpty(imagePath)) {
            try { sprite->ImageLocation = imagePath; }
            catch (...) {}
        }
    }

    property PictureBox^ Sprite{ PictureBox ^ get() { return sprite; } }
    property String^ Name{ String ^ get() { return name; } }
    property String^ Role{ String ^ get() { return role; } }

        // � C++/CLI ��� ref-������� ������ �������� default ��������.
        // ������ ��� ����������.
    void SmoothMoveTo(Point target);                 // ���������� � "���������" ���������
    void SmoothMoveTo(Point target, int moveSpeed);  // ��������
};

// ����������
inline void Human::SmoothMoveTo(Point target)
{
    // "���������" �������� = 5
    SmoothMoveTo(target, 5);
}

inline void Human::SmoothMoveTo(Point target, int moveSpeed)
{
    if (sprite == nullptr) return;
    int dx = (target.X > sprite->Left) ? moveSpeed : (target.X < sprite->Left ? -moveSpeed : 0);
    int dy = (target.Y > sprite->Top) ? moveSpeed : (target.Y < sprite->Top ? -moveSpeed : 0);
    sprite->Left += dx;
    sprite->Top += dy;
}

// ===== �������� =====

public ref class SecurityGuard : public Human
{
private:
    Timer^ guardTimer;
    bool queueDetected;

public:
    // ��������� ������� ������ ������� ��� �������������
    event GuardEventHandler^ OnLoadAllowed;

    // � ����� �������
    event EventHandler^ OnQueueReady;
    event EventHandler^ OnQueueEmpty;

    SecurityGuard(String^ imagePath)
        : Human("��������", "SecurityGuard", imagePath),
        guardTimer(gcnew Timer()),
        queueDetected(false)
    {
        guardTimer->Interval = 500;
        guardTimer->Tick += gcnew EventHandler(this, &SecurityGuard::OnGuardTick);
        guardTimer->Start();
    }

    // ����� ��� �������� �������
    void CheckQueue(List<Car^>^ cars, int queueX, int queueY)
    {
        if (cars == nullptr || cars->Count == 0) {
            if (queueDetected) {
                queueDetected = false;
                OnQueueEmpty(this, EventArgs::Empty);
            }
            return;
        }

        // ��������� ������� �� ������ ������ �� ���������
        Car^ firstCar = cars[0];
        bool firstCarAtGate = (Math::Abs(firstCar->Sprite->Left - queueX) <= 10) &&
            (Math::Abs(firstCar->Sprite->Top - queueY) <= 10);

        if (firstCarAtGate && !queueDetected) {
            queueDetected = true;
            OnQueueReady(this, EventArgs::Empty);
        }
        else if (!firstCarAtGate && queueDetected) {
            queueDetected = false;
            OnQueueEmpty(this, EventArgs::Empty);
        }
    }

    void OnGuardTick(Object^ sender, EventArgs^ e) {}

    void TriggerLoadAllowed()
    {
        OnLoadAllowed();
    }
};