#pragma once
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/custom/animations.h"
#include "ImGui/custom/notify.h"
#include "variables.hpp"
#include "includes.hpp"
#include "ImGui/custom/custom.h"
#ifndef DRAW_HPP
#define DRAW_HPP

void draw_menu()
{

    if (GetAsyncKeyState(VK_INSERT))
    {
        bDrawMenu = !bDrawMenu;
        Sleep(100);
    }

    ImGui::SetNextWindowSize({ 670,500 });

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0,0 });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3);

    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.2f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(ImColor(14, 14, 14, 255)));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14, 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14, 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20, 255)));

    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(19, 19, 19)));
    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(ImColor(40, 40, 40)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(14, 14, 14, 255)));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(250, 250, 250)));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, { 35.f / 255.f,35.f / 255.f,35.f / 255.f,0 / 255.f });

    long style = ::GetWindowLong(overlay->Window, GWL_EXSTYLE);

    auto window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
    static bool ShowNotification = true;
    static bool ShowMenu = true;
    static int tab = 0;

    int menu = Animate("menu", "menu", bDrawMenu, 255, 40.f, INTERP);
    menu = ImClamp(menu, 100, 255);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menu / 255.f);
    if (bDrawMenu)
    {
        style &= ~WS_EX_LAYERED;
        SetWindowLong(overlay->Window, GWL_EXSTYLE, style);
        SetFocus(overlay->Window);

        if (ShowNotification)
        {
            ImGui::InsertNotification({ ImGuiToastType_Success, 3000, "Hooked successfully!" });
            ShowNotification = false;
        }

       if (ImGui::Begin("prediction", &ShowMenu, window_flags))
       {

           if (ImGui::BeginChild(("##tabs"), ImVec2(160, 0), true, ImGuiWindowFlags_NoScrollbar))
           {
               ImGui::PushFont(subtitle);
               ImGui::SetCursorPos(ImVec2(15, 15));
               ImGui::Text("Aimbot");
               ImGui::PopFont();

               ImGui::SetCursorPosY(470);
               ImGui::SetCursorPosX(10);
               ImGui::Text("PredictionFN");

               int tab1alpha = Animate("Tab1", "Tab1", tab == 1, 255, 40.f, INTERP);
               tab1alpha = ImClamp(tab1alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab1alpha / 255.f);
               if (tab == 1)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 40));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.175f, 0.5f));
                   if (ImGui::Button("General", ImVec2(145, 30)))
                   {
                       tab = 1;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

                   ImGui::SetCursorPos(ImVec2(7, 45));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("  ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 40));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.175f, 0.5f));

                   if (ImGui::Button("General", ImVec2(145, 30)))
                   {
                       tab = 1;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

               int tab2alpha = Animate("Tab2", "Tab2", tab == 2, 255, 40.f, INTERP);
               tab2alpha = ImClamp(tab2alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab2alpha / 255.f);
               if (tab == 2)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 75));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   if (ImGui::Button("Misc", ImVec2(145, 30)))
                   {
                       tab = 2;
                   }
                   ImGui::PopStyleColor(4);

                   ImGui::SetCursorPos(ImVec2(7, 80));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("   ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 75));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   if (ImGui::Button("Misc", ImVec2(145, 30)))
                   {
                       tab = 2;
                   }
                   ImGui::PopStyleColor(4);

               }
               ImGui::PopStyleVar();


               ImGui::PushFont(subtitle);
               ImGui::SetCursorPos(ImVec2(15, 115));
               ImGui::Text("Visuals");
               ImGui::PopFont();

               int tab3alpha = Animate("Tab3", "Tab3", tab == 3, 255, 40.f, INTERP);
               tab3alpha = ImClamp(tab3alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab3alpha / 255.f);
               if (tab == 3)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 140));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.175f, 0.5f));

                   if (ImGui::Button("Players", ImVec2(145, 30)))
                   {
                       tab = 3;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

                   ImGui::SetCursorPos(ImVec2(7, 145));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("            ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 140));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.175f, 0.5f));

                   if (ImGui::Button("Players", ImVec2(145, 30)))
                   {
                       tab = 3;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

               int tab4alpha = Animate("Tab4", "Tab4", tab == 4, 255, 40.f, INTERP);
               tab4alpha = ImClamp(tab4alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab4alpha / 255.f);
               if (tab == 4)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 175));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("World", ImVec2(145, 30)))
                   {
                       tab = 4;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

                   ImGui::SetCursorPos(ImVec2(7, 180));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("    ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 175));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("World", ImVec2(145, 30)))
                   {
                       tab = 4;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

               int tab5alpha = Animate("Tab5", "Tab5", tab == 5, 255, 40.f, INTERP);
               tab5alpha = ImClamp(tab5alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab5alpha / 255.f);
               if (tab == 5)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 210));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Color", ImVec2(145, 30)))
                   {
                       tab = 5;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();
                   ImGui::SetCursorPos(ImVec2(7, 215));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("       ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 210));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Color", ImVec2(145, 30)))
                   {
                       tab = 5;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

               ImGui::PushFont(subtitle);
               ImGui::SetCursorPos(ImVec2(15, 250));
               ImGui::Text("Misc");
               ImGui::PopFont();

               int tab9alpha = Animate("Tab9", "Tab9", tab == 9, 255, 40.f, INTERP);
               tab9alpha = ImClamp(tab9alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab9alpha / 255.f);
               if (tab == 9)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 275));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Settings", ImVec2(145, 30)))
                   {
                       tab = 9;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

                   ImGui::SetCursorPos(ImVec2(7, 280));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("       ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 275));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14))); // 35
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Settings", ImVec2(145, 30)))
                   {
                       tab = 9;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

               int tab6alpha = Animate("Tab6", "Tab6", tab == 6, 255, 40.f, INTERP);
               tab6alpha = ImClamp(tab6alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab6alpha / 255.f);
               if (tab == 6)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 310));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Exploits", ImVec2(145, 30)))
                   {
                       tab = 6;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

                   ImGui::SetCursorPos(ImVec2(7, 315));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("       ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 310));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Exploits", ImVec2(145, 30)))
                   {
                       tab = 6;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

               int tab7alpha = Animate("Tab7", "Tab7", tab == 7, 255, 40.f, INTERP);
               tab7alpha = ImClamp(tab7alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab7alpha / 255.f);
               if (tab == 7)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 345));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Config", ImVec2(145, 30)))
                   {
                       tab = 7;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

                   ImGui::SetCursorPos(ImVec2(7, 350));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("       ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 345));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Config", ImVec2(145, 30)))
                   {
                       tab = 7;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

               int tab8alpha = Animate("Tab8", "Tab8", tab == 8, 255, 40.f, INTERP);
               tab8alpha = ImClamp(tab8alpha, 100, 255);
               ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab8alpha / 255.f);
               if (tab == 8)
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));
                   ImGui::SetCursorPos(ImVec2(7, 380));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(20, 20, 20)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Chat", ImVec2(145, 30)))
                   {
                       tab = 8;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

                   ImGui::SetCursorPos(ImVec2(7, 385));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(94, 92, 179)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(94, 92, 179)));
                   ImGui::Button("       ", ImVec2(5, 20));
                   ImGui::PopStyleColor(3);

               }
               else
               {
                   ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(14, 14, 14)));
                   ImGui::SetCursorPos(ImVec2(7, 380));
                   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor(14, 14, 14)));
                   ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.16f, 0.5f));

                   if (ImGui::Button("Chat", ImVec2(145, 30)))
                   {
                       tab = 8;
                   }
                   ImGui::PopStyleColor(4);
                   ImGui::PopStyleVar();

               }
               ImGui::PopStyleVar();

           }
           ImGui::EndChild();

            int tab1alpha = Animate("Tab1", "Tab1", tab == 1, 255, 40.f, INTERP);
            tab1alpha = ImClamp(tab1alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab1alpha / 255.f);
            if (tab == 1)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Aimbot");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("Aimbot");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &aimbot->bAimbot);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Aim Prediction");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("      ", &aimbot->prediction);

                    ImGui::SetCursorPos(ImVec2(10, 95));
                    ImGui::Text("Smoothing");
                    ImGui::SetCursorPos(ImVec2(10, 111));
                    ImGui::PushItemWidth(215);
                    ImGui::SliderInt("        ", &aimbot->smoothness, 1.f, 10.0f);

                    ImGui::SetCursorPos(ImVec2(10, 136));
                    ImGui::Text("Humanize Aimbot");
                    ImGui::SetCursorPos(ImVec2(195, 136));
                    Toggle("      ", &aimbot->humanization);

                    ImGui::SetCursorPos(ImVec2(10, 161));
                    ImGui::Text("SilentAim");
                    ImGui::SetCursorPos(ImVec2(195, 161));
                    Toggle("    ", &aimbot->bSilentAimbot);

                    ImGui::SetCursorPos(ImVec2(10, 186));
                    ImGui::Text("Aim Distance");
                    ImGui::SetCursorPos(ImVec2(10, 202));
                    ImGui::PushItemWidth(215);
                    ImGui::SliderFloat("        ", &aimbot->AimDistance, 1.f, 200.0f);

                    ImGui::SetCursorPos(ImVec2(10, 227));
                    ImGui::Text("Aim Line");
                    ImGui::SetCursorPos(ImVec2(195, 227));
                    Toggle("            ", &aimbot->Aimline);

                    ImGui::SetCursorPos(ImVec2(10, 252));
                    ImGui::Text("Draw Crosshair");
                    ImGui::SetCursorPos(ImVec2(195, 252));
                    Toggle("           ", &aimbot->crosshair);

                    ImGui::SetCursorPos(ImVec2(10, 277));
                    ImGui::Text("Draw FOV");
                    ImGui::SetCursorPos(ImVec2(195, 277));
                    Toggle(" ", &aimbot->showfov);

                    ImGui::SetCursorPos(ImVec2(10, 302));
                    ImGui::Text("Aim FOV Size");
                    ImGui::SetCursorPos(ImVec2(10, 318));
                    ImGui::PushItemWidth(215);
                    ImGui::SliderFloat("          ", &aimbot->fov, 1.f, 500.0f);

                    ImGui::EndChild();
                }

                ImGui::SetCursorPos(ImVec2(420, 13));

                if (ImGui::BeginChild(("##Triggerbot"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Triggerbot");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("Triggerbot");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &aimbot->triggerbot);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Triggerbot Speed");
                    ImGui::SetCursorPos(ImVec2(10, 86));
                    ImGui::PushItemWidth(215);
                    ImGui::SliderInt("        ", &aimbot->triggerSpeed, 1.f, 10.0f);

                    ImGui::EndChild();
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int tab2alpha = Animate("Tab2", "Tab2", tab == 2, 255, 40.f, INTERP);
            tab2alpha = ImClamp(tab2alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab2alpha / 255.f);
            if (tab == 2)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 280), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("General");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("Backtrack");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &aimbot->Backtrack);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Weapon Only");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle(" ", &aimbot->bWeapon);

                    ImGui::SetCursorPos(ImVec2(10, 95));
                    ImGui::Text("Woman only");
                    ImGui::SetCursorPos(ImVec2(195, 95));
                    Toggle("    ", &aimbot->bOnlyMale);

                    ImGui::EndChild();
                }

                ImGui::SetCursorPos(ImVec2(420, 13));
                if (ImGui::BeginChild(("##main2"), ImVec2(240, 130), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::PushFont(subtitle);
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Aimbone");
                    ImGui::PopFont();

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("Head");
                    ImGui::SetCursorPos(ImVec2(68, 45));
                    Toggle("    ", &aimbot->Head);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Neck");
                    ImGui::SetCursorPos(ImVec2(68, 70));
                    Toggle("    ", &aimbot->Neck);

                    ImGui::SetCursorPos(ImVec2(120, 70));
                    ImGui::Text("Chest");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("    ", &aimbot->Chest);

                    ImGui::SetCursorPos(ImVec2(120, 45));
                    ImGui::Text("Pevlis");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &aimbot->Pelvis);

                    ImGui::EndChild();
                }

                ImGui::SetCursorPos(ImVec2(420, 152));
                if (ImGui::BeginChild(("##main3"), ImVec2(240, 140), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::PushFont(subtitle);
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Keybind");
                    ImGui::PopFont();

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("LMouse");
                    ImGui::SetCursorPos(ImVec2(68, 45));
                    Toggle("    ", &aimbot->LMouse);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("RMouse");
                    ImGui::SetCursorPos(ImVec2(68, 70));
                    Toggle("    ", &aimbot->RMouse);

                    ImGui::SetCursorPos(ImVec2(120, 70));
                    ImGui::Text("LShift");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("    ", &aimbot->LShift);

                    ImGui::SetCursorPos(ImVec2(120, 45));
                    ImGui::Text("LCtrl");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &aimbot->LCtrl);

                    ImGui::EndChild();
                }

                ImGui::SetCursorPos(ImVec2(170, 300));
                if (ImGui::BeginChild(("##main4"), ImVec2(490, 190), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::PushFont(subtitle);
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Config");
                    ImGui::PopFont();

                    ImGui::Spacing();
                    ImGui::Separator();


                    ImGui::EndChild();
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int tab3alpha = Animate("Tab3", "Tab3", tab == 3, 255, 40.f, INTERP);
            tab3alpha = ImClamp(tab3alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab3alpha / 255.f);
            if (tab == 3)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Other");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45)); // 25
                    ImGui::Text("Boxes");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &visual->box);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Cornered box");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("    ", &visual->cornered_box);

                    ImGui::SetCursorPos(ImVec2(10, 95));
                    ImGui::Text("Outlined box");
                    ImGui::SetCursorPos(ImVec2(195, 95));
                    Toggle("    ", &visual->outline);

                    ImGui::SetCursorPos(ImVec2(10, 120));
                    ImGui::Text("Filled box");
                    ImGui::SetCursorPos(ImVec2(195, 120));
                    Toggle("    ", &visual->fill);

                    ImGui::SetCursorPos(ImVec2(10, 145));
                    ImGui::Text("ThreeD box");
                    ImGui::SetCursorPos(ImVec2(195, 145));
                    Toggle("    ", &visual->threed);

                    ImGui::SetCursorPos(ImVec2(10, 170));
                    ImGui::Text("Skeleton");
                    ImGui::SetCursorPos(ImVec2(195, 170));
                    Toggle("    ", &visual->gayskeleton);

                    ImGui::SetCursorPos(ImVec2(10, 195));
                    ImGui::Text("Snaplines");
                    ImGui::SetCursorPos(ImVec2(195, 195));
                    Toggle("    ", &visual->line);

                    ImGui::SetCursorPos(ImVec2(10, 220));
                    ImGui::Text("View Angles");
                    ImGui::SetCursorPos(ImVec2(195, 220));
                    Toggle("    ", &visual->bViewAngles);

                    ImGui::SetCursorPos(ImVec2(10, 245));
                    ImGui::Text("Chams");
                    ImGui::SetCursorPos(ImVec2(195, 245));
                    Toggle("    ", &visual->chams);

                    ImGui::SetCursorPos(ImVec2(10, 270));
                    ImGui::Text("Head");
                    ImGui::SetCursorPos(ImVec2(195, 270));
                    Toggle("    ", &visual->b2Dhead);

                    ImGui::SetCursorPos(ImVec2(10, 295));
                    ImGui::Text("Player Distance");
                    ImGui::SetCursorPos(ImVec2(10, 311));
                    ImGui::SliderFloat("    ", &visual->MaxDistance, 0.f, 500.f);

                    // visual->draw_radar

                    ImGui::EndChild();

                }

                ImGui::SetCursorPos(ImVec2(420, 13));

                if (ImGui::BeginChild(("##main2"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Text");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("Name");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &visual->name);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Distance");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("    ", &visual->distance);

                    ImGui::SetCursorPos(ImVec2(10, 95));
                    ImGui::Text("Console");
                    ImGui::SetCursorPos(ImVec2(195, 95));
                    Toggle("    ", &visual->console);

                    ImGui::SetCursorPos(ImVec2(10, 120));
                    ImGui::Text("Debug");
                    ImGui::SetCursorPos(ImVec2(195, 120));
                    Toggle("    ", &visual->PlayerDebug);

                    ImGui::SetCursorPos(ImVec2(10, 145));
                    ImGui::Text("Weapon");
                    ImGui::SetCursorPos(ImVec2(195, 145));
                    Toggle("    ", &visual->WeaponESP);

                    ImGui::SetCursorPos(ImVec2(10, 170));
                    ImGui::Text("Font Size");
                    ImGui::SetCursorPos(ImVec2(10, 186));
                    ImGui::SliderFloat("    ", &visual->size, 0.f, 500.f);

                    ImGui::EndChild();
                }


                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int tab4alpha = Animate("Tab4", "Tab4", tab == 4, 255, 40.f, INTERP);
            tab4alpha = ImClamp(tab4alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab4alpha / 255.f);
            if (tab == 4)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("General");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("Pickups");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &visual->loot->pickups);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Ammo");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("    ", &visual->loot->ammo);

                    ImGui::SetCursorPos(ImVec2(10, 95));
                    ImGui::Text("Chests");
                    ImGui::SetCursorPos(ImVec2(195, 95));
                    Toggle("    ", &visual->loot->chests);

                    ImGui::SetCursorPos(ImVec2(10, 120));
                    ImGui::Text("Supplys");
                    ImGui::SetCursorPos(ImVec2(195, 120));
                    Toggle("    ", &visual->loot->supply);

                    ImGui::SetCursorPos(ImVec2(10, 145));
                    ImGui::Text("Vehicles");
                    ImGui::SetCursorPos(ImVec2(195, 145));
                    Toggle("    ", &visual->loot->vehicle);

                    ImGui::SetCursorPos(ImVec2(10, 170));
                    ImGui::Text("Animals");
                    ImGui::SetCursorPos(ImVec2(195, 170));
                    Toggle("    ", &visual->loot->animal);

                    ImGui::SetCursorPos(ImVec2(10, 195));
                    ImGui::Text("Gernades");
                    ImGui::SetCursorPos(ImVec2(195, 195));
                    Toggle("    ", &visual->loot->generades);

                    ImGui::SetCursorPos(ImVec2(10, 220));
                    ImGui::Text("Rifts");
                    ImGui::SetCursorPos(ImVec2(195, 220));
                    Toggle("    ", &visual->loot->rift);

                    ImGui::SetCursorPos(ImVec2(10, 245));
                    ImGui::Text("WeakSpots");
                    ImGui::SetCursorPos(ImVec2(195, 245));
                    Toggle("    ", &visual->loot->weakspot);

                    ImGui::SetCursorPos(ImVec2(10, 270));
                    ImGui::Text("Loot Distance");
                    ImGui::SetCursorPos(ImVec2(10, 281));
                    ImGui::SliderFloat("    ", &visual->loot->MaxItemDistance, 0.f, 200.f);

                    ImGui::EndChild();
                }

                ImGui::SetCursorPos(ImVec2(420, 13));
                if (ImGui::BeginChild(("##main2"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Type");

                    ImGui::Spacing();
                    ImGui::Separator();

                    //ImGui::SetCursorPos(ImVec2(10, 45));
                    //ImGui::Text("Boxes");
                    //ImGui::SetCursorPos(ImVec2(195, 45));
                    //Toggle("    ", &visual->loot->item_box);

                    //ImGui::SetCursorPos(ImVec2(10, 70));
                    //ImGui::Text("ThreeD boxes");
                    //ImGui::SetCursorPos(ImVec2(195, 70));
                    //Toggle("    ", &visual->loot->item_3dbox);

                    //ImGui::SetCursorPos(ImVec2(10, 95));
                    //ImGui::Text("Snaplines");
                    //ImGui::SetCursorPos(ImVec2(195, 95));
                    //Toggle("    ", &visual->loot->item_line);

                    //ImGui::SetCursorPos(ImVec2(10, 120));
                    //ImGui::Text("Name");
                    //ImGui::SetCursorPos(ImVec2(195, 120));
                    //Toggle("    ", &visual->loot->item_text);

                    //ImGui::SetCursorPos(ImVec2(10, 145));
                    //ImGui::Text("Distance");
                    //ImGui::SetCursorPos(ImVec2(195, 145));
                    //Toggle("    ", &visual->loot->item_text_distance);

                    ImGui::EndChild();
                }

                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int tab5alpha = Animate("Tab5", "Tab5", tab == 5, 255, 40.f, INTERP);
            tab5alpha = ImClamp(tab5alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab5alpha / 255.f);
            if (tab == 5)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 330), true, ImGuiWindowFlags_NoScrollbar))
                {
                    auto flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip |
                        ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions;

                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Colors");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPosX(10);
                    ImGui::Text(("Visible player"));
                    ImGui::SetCursorPosX(10);
                    ImGui::ColorEdit4("Box", visual->color->PlayerboxVisible, flags);
                    ImGui::SetCursorPosX(10);
                    if (!misc->bIgnoreTeam) ImGui::ColorEdit4("Team Box", visual->color->TeamboxVisible, flags);
                    ImGui::SetCursorPosX(10);
                    ImGui::ColorEdit4("Skeleton", visual->color->SkeletonVisible, flags);
                    ImGui::SetCursorPosX(10);
                    ImGui::ColorEdit4("Snapline", visual->color->SnaplinesVisible, flags);

                    ImGui::Text("           ");

                    ImGui::SetCursorPosX(10);
                    if (!misc->bIgnoreHidden) ImGui::Text(("Not visible player"));
                    ImGui::SetCursorPosX(10);
                    if (!misc->bIgnoreHidden) ImGui::ColorEdit4(("Box "), visual->color->PlayerboxNotVisible, flags);
                    ImGui::SetCursorPosX(10);
                    if (!misc->bIgnoreHidden && !misc->bIgnoreTeam) ImGui::ColorEdit4(("Team Box "), visual->color->TeamboxNotVisible, flags);
                    ImGui::SetCursorPosX(10);
                    if (!misc->bIgnoreHidden) ImGui::ColorEdit4(("Skeleton "), visual->color->SkeletonNotVisible, flags);
                    ImGui::SetCursorPosX(10);
                    if (!misc->bIgnoreHidden)  ImGui::ColorEdit4(("Snapline "), visual->color->SnaplinesNotVisible, flags);

                    ImGui::EndChild();
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int tab6alpha = Animate("Tab6", "Tab6", tab == 6, 255, 40.f, INTERP);
            tab6alpha = ImClamp(tab6alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab6alpha / 255.f);
            if (tab == 6)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Exploits");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::Checkbox("Player fly", &exploits->cheat_flying);
                    ImGui::Checkbox("Unlock fire rate", &exploits->fire_rate);
                    ImGui::Checkbox("No spread", &exploits->no_spread);
                    ImGui::Checkbox("Instant reload", &exploits->instant_reload);
                    ImGui::Checkbox("No clip", &exploits->no_clip);
                    ImGui::Checkbox("No over heat", &exploits->no_over_heat);
                    ImGui::Checkbox("ADS in air", &exploits->ads_in_air);
                    ImGui::Checkbox("No recoil", &exploits->no_recoil);
                    ImGui::Checkbox("Magic bullet", &exploits->magic_bullet);

                    ImGui::EndChild();
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int tab22alpha = Animate("Tab22", "Tab22", tab == 7, 255, 40.f, INTERP);
            tab22alpha = ImClamp(tab22alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab22alpha / 255.f);
            if (tab == 7)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Config");

                    ImGui::Spacing();
                    ImGui::Separator();



                    ImGui::EndChild();
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int tab23alpha = Animate("Tab23", "Tab23", tab == 8, 255, 40.f, INTERP);
            tab23alpha = ImClamp(tab23alpha, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab23alpha / 255.f);
            if (tab == 8)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 470), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Chat");

                    ImGui::Spacing();
                    ImGui::Separator();


                    ImGui::EndChild();
                }
                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

            int nigger = Animate("nigger", "nigger", tab == 9, 255, 40.f, INTERP);
            nigger = ImClamp(nigger, 100, 255);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, nigger / 255.f);
            if (tab == 9)
            {
                ImGui::SetCursorPos(ImVec2(170, 13));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(ImColor(15, 15, 15, 255)));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(ImColor(40, 40, 40)));

                if (ImGui::BeginChild(("##main1"), ImVec2(240, 230), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Settings");

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("StreamProof");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &misc->StreamProof);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("VSync");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("    ", &misc->VSync);

                    ImGui::SetCursorPos(ImVec2(10, 95));
                    ImGui::Text("Preformence Mode");
                    ImGui::SetCursorPos(ImVec2(195, 95));
                    Toggle("    ", &misc->PreformenceMode);

                    ImGui::SetCursorPos(ImVec2(10, 115));
                    ImGui::Text("Exit on bind");
                    ImGui::SetCursorPos(ImVec2(195, 115));
                    Toggle("    ", &misc->ExitThread);

                    ImGui::EndChild();
                }

                ImGui::SetCursorPos(ImVec2(420, 13));
                if (ImGui::BeginChild(("##main2"), ImVec2(240, 280), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::PushFont(subtitle);
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::Text("Player Checks");
                    ImGui::PopFont();

                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::SetCursorPos(ImVec2(10, 45));
                    ImGui::Text("Team check");
                    ImGui::SetCursorPos(ImVec2(195, 45));
                    Toggle("    ", &misc->bIgnoreTeam);

                    ImGui::SetCursorPos(ImVec2(10, 70));
                    ImGui::Text("Downed check");
                    ImGui::SetCursorPos(ImVec2(195, 70));
                    Toggle("    ", &misc->bIgnoreDead);

                    ImGui::SetCursorPos(ImVec2(10, 95));
                    ImGui::Text("Hidden check");
                    ImGui::SetCursorPos(ImVec2(195, 95));
                    Toggle("    ", &misc->bIgnoreHidden);

                    ImGui::SetCursorPos(ImVec2(10, 120));
                    ImGui::Text("Bot check");
                    ImGui::SetCursorPos(ImVec2(195, 120));
                    Toggle("    ", &misc->bIgnoreBots);

                    ImGui::SetCursorPos(ImVec2(10, 145));
                    ImGui::Text("AFK check");
                    ImGui::SetCursorPos(ImVec2(195, 145));
                    Toggle("    ", &misc->bIgnoreAFK);

                    ImGui::EndChild();
                }

                ImGui::PopStyleColor(2);
            }
            ImGui::PopStyleVar();

        }
        ImGui::End();
    }
    else
    {
        style |= WS_EX_LAYERED;
        SetWindowLong(overlay->Window, GWL_EXSTYLE, style);
        SetWindowLong(overlay->Window, GWL_EXSTYLE, style);
        SetFocus(overlay->Window);
    }

    ImGui::PopStyleVar();
}

#endif // DRAW_HPP