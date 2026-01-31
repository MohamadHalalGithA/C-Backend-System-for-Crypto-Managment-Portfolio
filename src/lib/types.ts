// Types matching the C backend data structures

export interface Asset {
  id: string;
  user_id: string;
  symbol: string;
  name: string;
  quantity: number;
  current_price: number;
  avg_cost: number;
  category: string;
  created_at: string;
  updated_at: string;
}

export interface Transaction {
  id: string;
  user_id: string;
  type: 'buy' | 'sell';
  asset: string;
  amount: number;
  price: number;
  total: number;
  timestamp: string;
  status: 'pending' | 'completed' | 'failed';
}

export interface ChartDataPoint {
  date: string;
  value: number;
  profit: number;
}

export interface AllocationData {
  name: string;
  value: number;
  color: string;
}

export interface User {
  id: string;
  email: string;
  username: string;
}

export interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
}
