import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { api } from '@/lib/api';
import type { Asset, Transaction } from '@/lib/types';

// Fallback mock data when backend is unavailable
const mockAssets: Asset[] = [
  { id: "1", user_id: "1", symbol: "BTC", name: "Bitcoin", quantity: 2.5, current_price: 43250.00, avg_cost: 38000.00, category: "Crypto", created_at: "", updated_at: "" },
  { id: "2", user_id: "1", symbol: "ETH", name: "Ethereum", quantity: 15.0, current_price: 2650.00, avg_cost: 2200.00, category: "Crypto", created_at: "", updated_at: "" },
  { id: "3", user_id: "1", symbol: "AAPL", name: "Apple Inc.", quantity: 50, current_price: 185.50, avg_cost: 165.00, category: "Stocks", created_at: "", updated_at: "" },
  { id: "4", user_id: "1", symbol: "GOLD", name: "Gold", quantity: 10, current_price: 2045.00, avg_cost: 1950.00, category: "Commodities", created_at: "", updated_at: "" },
];

const mockTransactions: Transaction[] = [
  { id: "1", user_id: "1", type: "buy", asset: "BTC", amount: 0.5, price: 42000, total: 21000, timestamp: "2024-01-15T10:30:00Z", status: "completed" },
  { id: "2", user_id: "1", type: "sell", asset: "ETH", amount: 5.0, price: 2600, total: 13000, timestamp: "2024-01-14T14:22:00Z", status: "completed" },
  { id: "3", user_id: "1", type: "buy", asset: "AAPL", amount: 25, price: 182, total: 4550, timestamp: "2024-01-13T09:15:00Z", status: "pending" },
];

const mockChartData = [
  { date: "Jan", value: 45000, profit: 2000 }, { date: "Feb", value: 52000, profit: 3500 },
  { date: "Mar", value: 48000, profit: 1500 }, { date: "Apr", value: 61000, profit: 5000 },
  { date: "May", value: 58000, profit: 4200 }, { date: "Jun", value: 72000, profit: 8000 },
];

const mockAllocation = [
  { name: "Crypto", value: 45, color: "hsl(var(--primary))" },
  { name: "Stocks", value: 30, color: "hsl(var(--accent))" },
  { name: "Commodities", value: 15, color: "hsl(180, 100%, 50%)" },
  { name: "Forex", value: 10, color: "hsl(280, 100%, 60%)" },
];

function isBEUnavailable(error: unknown) {
  return error instanceof Error && error.message.includes("Backend unavailable");
}

export function useAssets() {
  return useQuery({
    queryKey: ['assets'],
    queryFn: async () => {
      try {
        return await api.getAssets();
      } catch (error) {
        if (isBEUnavailable(error)) return mockAssets;
        throw error;
      }
    },
    staleTime: 30000,
  });
}

export function useTransactions() {
  return useQuery({
    queryKey: ['transactions'],
    queryFn: async () => {
      try {
        return await api.getTransactions();
      } catch (error) {
        if (isBEUnavailable(error)) return mockTransactions;
        throw error;
      }
    },
    staleTime: 30000,
  });
}

export function useChartData() {
  return useQuery({
    queryKey: ['chartData'],
    queryFn: async () => {
      try {
        return await api.getChartData();
      } catch (error) {
        if (isBEUnavailable(error)) return mockChartData;
        throw error;
      }
    },
    staleTime: 60000,
  });
}

export function useAllocation() {
  return useQuery({
    queryKey: ['allocation'],
    queryFn: async () => {
      try {
        return await api.getAllocation();
      } catch (error) {
        if (isBEUnavailable(error)) return mockAllocation;
        throw error;
      }
    },
    staleTime: 60000,
  });
}

export function useCreateAsset() {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: (asset: Omit<Asset, 'id' | 'user_id' | 'created_at' | 'updated_at'>) => 
      api.createAsset(asset),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['assets'] });
      queryClient.invalidateQueries({ queryKey: ['allocation'] });
    },
  });
}

export function useDeleteAsset() {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: (id: string) => api.deleteAsset(id),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['assets'] });
      queryClient.invalidateQueries({ queryKey: ['allocation'] });
    },
  });
}

export function useCreateTransaction() {
  const queryClient = useQueryClient();
  
  return useMutation({
    mutationFn: (tx: Omit<Transaction, 'id' | 'user_id'>) => api.createTransaction(tx),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['transactions'] });
      queryClient.invalidateQueries({ queryKey: ['assets'] });
      queryClient.invalidateQueries({ queryKey: ['chartData'] });
    },
  });
}
