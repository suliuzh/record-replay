
//--------------------------------------------------------------------------------------------------
/// @file bank_account.cpp
/// @author Susanne van den Elsen
/// @date 2017
//--------------------------------------------------------------------------------------------------

#include <atomic>
#include <cassert>
#include <thread>

//--------------------------------------------------------------------------------------------------

using Account = std::atomic<int>;

void transfer(Account& from, int amount, Account& to)
{
   const auto balance_from = from.load();
   if (balance_from >= amount)
   {
      to.fetch_add(amount);
      from.fetch_sub(amount);
   }
}

//--------------------------------------------------------------------------------------------------

int main()
{
   Account account1(100);
   Account account2(0);
   
   std::thread mallory(transfer, ref(account1), 100, ref(account2));
   std::thread marvin(transfer, ref(account1), 100, ref(account2));
   
   mallory.join();
   marvin.join();

   assert(account1.load() >= 0 && account2.load() >= 0);

   return 0;
}

//--------------------------------------------------------------------------------------------------
